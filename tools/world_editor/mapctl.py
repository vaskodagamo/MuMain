#!/usr/bin/env python3
"""mapctl: drive the Map Editor of a running editor client through its control socket.

For AI agents (Claude, Codex) and scripts that edit maps on the owner's prompt; the guide is
docs/agents/AI_MAP_EDITING.md, the commands behind it docs/control-socket.md ("Editor commands").
Python 3.9+, standard library only. Run it from anywhere:

    python3 tools/world_editor/mapctl.py launch --world 1     # start the editor build offline on Lorencia
    python3 tools/world_editor/mapctl.py info                 # map-info
    python3 tools/world_editor/mapctl.py shot /tmp/top.png --topdown 195 110 235 150
    python3 tools/world_editor/mapctl.py dry-run plan.json    # map-apply with dry_run
    python3 tools/world_editor/mapctl.py apply plan.json
    python3 tools/world_editor/mapctl.py quit

`mapctl.py <command> --help` lists a command's options. Every command prints the client's answer
(its `result` object) as JSON and exits 0. Exit 1: the client refused ({"ok": false, "error": ...,
"message": ...} on stdout, the message on stderr). Exit 2: bad arguments or input files. Exit 3:
no client listens on the socket, or it did not answer in time.

The socket is --socket, else $MU_CONTROL_SOCKET, else mu-mapctl.sock in a folder only you can
use: $XDG_RUNTIME_DIR, else /tmp/mu-<your uid>/ (the default of `launch` too; it creates the
folder). mapctl talks only to a socket file you own. Tiles are [x, y] and rectangles
[x0, y0, x1, y1] with both corners included, 0 to 255; `--map N` is the game's map number
(Lorencia 0), `--world N` its Data/World folder (N + 1). Paths given to the client
(screenshots, exports, scripts) are made absolute here first, because the client resolves
relative paths against its own working folder.

As a module (tools/world_editor on sys.path):

    import mapctl
    with mapctl.Client(mapctl.default_socket_path()) as client:
        info = client.call('map-info')
        report = client.call('map-apply', script=script, dry_run=True)
"""

import argparse
import json
import os
from pathlib import Path
import socket
import stat
import subprocess
import sys
import tempfile
import time

import map_sketch

SOCKET_ENVIRONMENT = 'MU_CONTROL_SOCKET'
DEFAULT_SOCKET_NAME = 'mu-mapctl.sock'
RUNTIME_DIR_ENVIRONMENT = 'XDG_RUNTIME_DIR'
PRIVATE_FOLDER_PARENT = '/tmp'
PRIVATE_FOLDER_PATTERN = 'mu-%d'
PRIVATE_FOLDER_MODE = 0o700
GROUP_OR_OTHER_BITS = 0o077
DEFAULT_TIMEOUT_SECONDS = 60.0
LAUNCH_TIMEOUT_SECONDS = 180.0
QUIT_WAIT_SECONDS = 20.0
PING_TIMEOUT_SECONDS = 2.0
POLL_SECONDS = 0.5
STOP_WAIT_SECONDS = 10.0
# A screenshot whose frame could not be read back (the window was being covered, a save
# held the frame) is asked for again this many times.
SHOT_RETRIES = 2
SHOT_RETRY_CODES = ('failed', 'busy')
SHOT_RETRY_PAUSE_SECONDS = 0.5

# The client closes a connection whose request line grows past 256 KiB (LocalSocket.h,
# MaxPendingInputBytes). A bigger edit script goes by path instead.
MAX_REQUEST_BYTES = 256 * 1024
MAX_RESPONSE_BYTES = 64 * 1024 * 1024
RECEIVE_CHUNK_BYTES = 64 * 1024
LINE_END = b'\n'

# sockaddr_un.sun_path minus its terminating zero.
SOCKET_PATH_LIMIT_MACOS = 103
SOCKET_PATH_LIMIT_OTHER = 107

EXIT_OK = 0
EXIT_REFUSED = 1
EXIT_USAGE = 2
EXIT_UNREACHABLE = 3

FIRST_WORLD_FOLDER = 1
LAST_WORLD_FOLDER = 255
WORLD_SCENE = 'world'
PNG_SUFFIX = '.png'
STDIN_NAME = '-'
LOG_FOLDER = Path('out') / 'mapctl'
CLIENT_LOG_NAME = 'MuError.log'
MACOS_LAUNCH_ARGUMENTS = ['-ApplePersistenceIgnoreState', 'YES']

EDITOR_PRESETS = {
    'darwin': ('macos-arm64-mueditor',),
    'linux': ('linux-x64-mueditor',),
    'win32': ('windows-x64-mueditor', 'windows-x86-mueditor'),
}
BUILD_CONFIGS = ('Release', 'RelWithDebInfo', 'Debug')
MAIN_CANDIDATES = (Path('Main.app') / 'Contents' / 'MacOS' / 'Main', Path('Main'), Path('Main.exe'))

MAP_FILES = ('texture', 'height', 'attribute', 'light', 'objects')
BLANK_OPTIONS = ('height', 'texture', 'attribute', 'light', 'textures_from_map', 'textures_from_world')
SAVE_CHOICES = ('all',) + MAP_FILES
EXPORT_LAYERS = ('height', 'attribute', 'texture1', 'texture2', 'alpha', 'light', 'objects')


# ---------------------------------------------------------------------------------------------
# Errors


class MapCtlError(Exception):
    """Base error; `code` and `exit_code` say what kind of failure it is."""

    code = 'bad_input'
    exit_code = EXIT_USAGE

    def __init__(self, message, code=None, result=None):
        super().__init__(message)
        if code is not None:
            self.code = code
        self.result = result

    def as_json(self):
        answer = {'ok': False, 'error': self.code, 'message': str(self)}
        if self.result:
            answer['result'] = self.result
        return answer


class CommandError(MapCtlError):
    """The client answered with ok: false (its error code is `code`)."""

    code = 'failed'
    exit_code = EXIT_REFUSED


class Unreachable(MapCtlError):
    """No client listens on the socket, or the connection broke."""

    code = 'unreachable'
    exit_code = EXIT_UNREACHABLE


class NoAnswer(Unreachable):
    code = 'no_answer'


class ProtocolError(Unreachable):
    code = 'protocol'


class UsageError(MapCtlError):
    """The command line itself is wrong (argparse's complaint)."""

    code = 'bad_arguments'


class ArgumentParser(argparse.ArgumentParser):
    """argparse that raises UsageError, so main() can answer in JSON (also with --compact)."""

    def error(self, message):
        raise UsageError('%s (%s)' % (message, self.format_usage().strip()))


# ---------------------------------------------------------------------------------------------
# Framing: one JSON object per line each way


def encode_request(request, request_id=None):
    """The wire form of a request: compact ASCII JSON and a newline; `id` is echoed back."""
    if not isinstance(request, dict) or not isinstance(request.get('cmd'), str) or not request['cmd']:
        raise MapCtlError('a request is a JSON object with a "cmd" string')
    message = dict(request)
    if request_id is not None:
        message['id'] = request_id
    line = json.dumps(message, separators=(',', ':'), ensure_ascii=True).encode('ascii') + LINE_END
    if len(line) > MAX_REQUEST_BYTES:
        raise MapCtlError('the request is %d bytes; the client takes lines of up to %d bytes '
                          '(send a big script by path)' % (len(line), MAX_REQUEST_BYTES))
    return line


def decode_response(line):
    """One response line to its object; raises ProtocolError for anything else."""
    try:
        response = json.loads(line.decode('utf-8'))
    except (UnicodeDecodeError, ValueError) as error:
        raise ProtocolError('the client sent a line that is not JSON: %s' % error)
    if not isinstance(response, dict) or not isinstance(response.get('ok'), bool):
        raise ProtocolError('the client sent a line without "ok": %.200s' % line)
    return response


class LineBuffer:
    """Collects received bytes and hands out complete lines (without the newline)."""

    def __init__(self, limit=MAX_RESPONSE_BYTES):
        self.limit = limit
        self.pending = bytearray()

    def feed(self, data):
        self.pending.extend(data)
        lines = []
        while True:
            end = self.pending.find(LINE_END)
            if end < 0:
                break
            line = bytes(self.pending[:end]).strip()
            del self.pending[:end + 1]
            if line:
                lines.append(line)
        if len(self.pending) > self.limit:
            raise ProtocolError('an answer grew past %d bytes without ending' % self.limit)
        return lines


def response_matches(response, request_id):
    """An answer without `id` is the client's answer to a line it could not parse, so ours too."""
    return 'id' not in response or response['id'] == request_id


# ---------------------------------------------------------------------------------------------
# Client


def private_folder():
    """A folder for sockets only this user can reach: $XDG_RUNTIME_DIR, else /tmp/mu-<uid>."""
    if os.name != 'posix':
        return tempfile.gettempdir()
    runtime = os.environ.get(RUNTIME_DIR_ENVIRONMENT)
    if runtime:
        return runtime
    return os.path.join(PRIVATE_FOLDER_PARENT, PRIVATE_FOLDER_PATTERN % os.getuid())


def default_socket_path():
    configured = os.environ.get(SOCKET_ENVIRONMENT)
    if configured:
        return configured
    return os.path.join(private_folder(), DEFAULT_SOCKET_NAME)


def prepare_socket_folder(socket_path):
    """Creates the socket's folder (only you may enter it); refuses one another user owns."""
    folder = os.path.dirname(socket_path)
    if not os.path.exists(folder):
        os.makedirs(folder, mode=PRIVATE_FOLDER_MODE, exist_ok=True)
    if not hasattr(os, 'getuid') or folder != private_folder():
        return
    info = os.lstat(folder)
    private = (stat.S_ISDIR(info.st_mode) and info.st_uid == os.getuid() and
               info.st_mode & GROUP_OR_OTHER_BITS == 0)
    if not private:
        raise MapCtlError('%s is not a folder only you can use (another user may have made it); remove it or '
                          'pass --socket' % folder, code='foreign_socket')


def check_socket_owner(path):
    """A socket file another user made could be anyone's program: mapctl does not talk to it."""
    if not hasattr(os, 'getuid'):
        return
    try:
        info = os.lstat(path)
    except OSError:
        return
    if info.st_uid != os.getuid():
        raise Unreachable('%s belongs to another user (uid %d), not to you: mapctl talks only to sockets you own'
                          % (path, info.st_uid), code='foreign_socket')


def resolve_socket_path(path=None):
    return os.path.abspath(path or default_socket_path())


class Client:
    """One connection to the client's control socket; requests are answered in order."""

    def __init__(self, socket_path=None, timeout=DEFAULT_TIMEOUT_SECONDS):
        self.socket_path = resolve_socket_path(socket_path)
        self.timeout = timeout
        self.connection = None
        self.buffer = LineBuffer()
        self.next_id = 0

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *exception):
        self.close()

    def connect(self):
        if self.connection is not None:
            return
        if not hasattr(socket, 'AF_UNIX'):
            raise Unreachable('this Python has no AF_UNIX sockets (CPython on Windows has none); mapctl needs '
                              'one that has them, as on macOS and Linux')
        check_socket_owner(self.socket_path)
        connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        connection.settimeout(self.timeout)
        try:
            connection.connect(self.socket_path)
        except FileNotFoundError:
            connection.close()
            raise Unreachable('no control socket at %s: start the editor with `mapctl.py launch --world N`, '
                              'or pass the socket the client was started with (--socket). If a client was '
                              'running there, %s says why it left ("Quit requested: ...")'
                              % (self.socket_path, client_log_hint()))
        except OSError as error:
            connection.close()
            raise Unreachable('cannot connect to %s (%s): nothing listens there; a client that crashed leaves '
                              'the file behind (%s says what happened), start one with `mapctl.py launch`'
                              % (self.socket_path, error, client_log_hint()))
        self.connection = connection
        self.buffer = LineBuffer()

    def close(self):
        if self.connection is not None:
            self.connection.close()
            self.connection = None

    def request(self, request, timeout=None):
        """Send one request object and return the whole response object."""
        self.connect()
        self.next_id += 1
        request_id = self.next_id
        wait = timeout if timeout is not None else self.timeout
        deadline = time.monotonic() + wait
        try:
            self.connection.settimeout(wait)
            self.connection.sendall(encode_request(request, request_id))
            return self._read_response(request_id, deadline)
        except socket.timeout:
            self.close()
            raise NoAnswer('no answer to `%s` within %g s: the client may still be loading or be busy; its log '
                           'is MuError.log next to Main' % (request['cmd'], wait))
        except OSError as error:
            self.close()
            raise Unreachable('the connection to %s broke (%s): the client quit or crashed'
                              % (self.socket_path, error))

    def _read_response(self, request_id, deadline):
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise socket.timeout()
            self.connection.settimeout(remaining)
            data = self.connection.recv(RECEIVE_CHUNK_BYTES)
            if not data:
                self.close()
                raise Unreachable('the client closed the connection without an answer (it quit or crashed)')
            for line in self.buffer.feed(data):
                response = decode_response(line)
                if response_matches(response, request_id):
                    return response

    def call(self, cmd, timeout=None, **fields):
        """Send `cmd` with `fields`; return its `result`, or raise CommandError."""
        return result_of(self.request(dict(fields, cmd=cmd), timeout))


def client_log_hint():
    """Where the editor build writes its log (MuError.log beside Main), for error messages."""
    try:
        return str(find_editor_main().parent / CLIENT_LOG_NAME)
    except MapCtlError:
        return CLIENT_LOG_NAME + ' beside Main'


def result_of(response):
    if response['ok']:
        return response.get('result', {})
    raise CommandError(response.get('message', ''), code=response.get('error', 'failed'),
                       result=response.get('result'))


# ---------------------------------------------------------------------------------------------
# Launching the editor build


def repo_root():
    return Path(__file__).resolve().parents[2]


def find_editor_main(root=None, platform=None):
    """The first editor build's Main found below <root>/out/build: this platform's presets, Release first."""
    root = Path(root) if root is not None else repo_root()
    platform = platform or sys.platform
    presets = EDITOR_PRESETS.get(platform) or tuple(preset for group in EDITOR_PRESETS.values() for preset in group)
    tried = []
    for preset in presets:
        for config in BUILD_CONFIGS:
            for candidate in MAIN_CANDIDATES:
                path = root / 'out' / 'build' / preset / 'src' / config / candidate
                tried.append(path)
                if path.is_file():
                    return path
    raise MapCtlError('no editor build found (looked for %s and %d more); build one with '
                      '`cmake --preset macos-arm64-mueditor` and `cmake --build --preset '
                      'macos-arm64-mueditor-release --target Main`, or pass --main' % (tried[0], len(tried) - 1),
                      code='no_build')


def check_socket_path(path, platform=None):
    platform = platform or sys.platform
    limit = SOCKET_PATH_LIMIT_MACOS if platform == 'darwin' else SOCKET_PATH_LIMIT_OTHER
    size = len(os.fsencode(path))
    if size > limit:
        raise MapCtlError('the socket path %s is %d bytes; the system takes %d at most (use a short path '
                          'such as /tmp/mu-editor.sock)' % (path, size, limit))


def check_world(world):
    if not FIRST_WORLD_FOLDER <= world <= LAST_WORLD_FOLDER:
        raise MapCtlError('--world is a Data/World folder number from %d to %d (map number + 1)'
                          % (FIRST_WORLD_FOLDER, LAST_WORLD_FOLDER))


def launch_command(main, world, platform=None):
    platform = platform or sys.platform
    command = [str(main), '--editor', '--world', str(world)]
    return command + (MACOS_LAUNCH_ARGUMENTS if platform == 'darwin' else [])


def ping(socket_path):
    """The ping answer of a client serving `socket_path`, or None."""
    try:
        with Client(socket_path, PING_TIMEOUT_SECONDS) as client:
            return client.call('ping')
    except (Unreachable, CommandError):
        return None


def serving_client(socket_path):
    """Something listens on `socket_path`: its ping answer, or {} when it takes the connection but does not
    answer in time (busy: a long map-open or edit, a dialog, a stalled window). None when nothing listens."""
    client = Client(socket_path, PING_TIMEOUT_SECONDS)
    try:
        client.connect()
    except Unreachable:
        return None
    try:
        return client.call('ping')
    except (Unreachable, CommandError):
        return {}
    finally:
        client.close()


def _start_process(command, cwd, socket_path, log_path):
    environment = dict(os.environ, **{SOCKET_ENVIRONMENT: socket_path})
    options = {'start_new_session': True} if os.name == 'posix' else {
        'creationflags': subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS}
    with open(log_path, 'ab') as log:
        return subprocess.Popen(command, cwd=cwd, env=environment, stdin=subprocess.DEVNULL, stdout=log,
                                stderr=subprocess.STDOUT, **options)


def _stop_process(process):
    process.terminate()
    try:
        process.wait(STOP_WAIT_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()


def answered_by_other(answer, pid):
    """The ping answer names another process than `pid` (clients that do not say count as ours)."""
    return answer.get('pid') not in (None, pid)


def _wait_for_world(process, socket_path, timeout, log_path):
    deadline = time.monotonic() + timeout
    while True:
        code = process.poll()
        if code is not None:
            raise Unreachable('the client exited with code %d before it answered; see %s and %s'
                              % (code, log_path, Path(process.args[0]).parent / CLIENT_LOG_NAME), code='exited')
        answer = ping(socket_path)
        if answer is not None and answered_by_other(answer, process.pid):
            _stop_process(process)
            raise CommandError('another client (pid %s) took %s first; the one started here (pid %d) was stopped'
                               % (answer.get('pid'), socket_path, process.pid), code='already_running',
                               result=answer)
        if answer is not None and answer.get('scene') == WORLD_SCENE:
            return answer
        if time.monotonic() > deadline:
            _stop_process(process)
            raise NoAnswer('the client did not reach the world scene within %g s and was stopped (pid %d); '
                           'see %s' % (timeout, process.pid, log_path))
        time.sleep(POLL_SECONDS)


def launch(world, socket_path=None, main=None, timeout=LAUNCH_TIMEOUT_SECONDS, log_path=None):
    """Start the editor build offline on Data/World{world} with the control socket; wait for it."""
    check_world(world)
    socket_path = resolve_socket_path(socket_path)
    check_socket_path(socket_path)
    prepare_socket_folder(socket_path)
    running = serving_client(socket_path)
    if running is not None:
        state = 'scene %s, pid %s' % (running.get('scene'), running.get('pid')) if running else 'busy: no answer'
        raise CommandError('a client already serves %s (%s); quit it first or pick another --socket'
                           % (socket_path, state), code='already_running', result=running or None)
    main = Path(main).resolve() if main else find_editor_main()
    if not main.is_file():
        raise MapCtlError('%s is not a file' % main, code='no_build')
    log_path = Path(log_path) if log_path else repo_root() / LOG_FOLDER / (Path(socket_path).stem + '.log')
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    process = _start_process(launch_command(main, world), main.parent, socket_path, log_path)
    answer = _wait_for_world(process, socket_path, timeout, log_path)
    return {'pid': process.pid, 'socket': socket_path, 'world': world, 'map': world - 1, 'main': str(main),
            'log': str(log_path), 'client_log': str(main.parent / CLIENT_LOG_NAME),
            'seconds': round(time.monotonic() - started, 1), 'ping': answer}


def wait_until_gone(path, seconds):
    """True once the socket file is gone (the client removes it when it exits)."""
    deadline = time.monotonic() + seconds
    while os.path.exists(path):
        if time.monotonic() > deadline:
            return False
        time.sleep(POLL_SECONDS)
    return True


def process_alive(pid):
    """The process runs (a child of this one that has ended is reaped, so it does not count)."""
    try:
        if os.waitpid(pid, os.WNOHANG)[0] == pid:
            return False
    except ChildProcessError:
        pass
    except OSError:
        return True
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def wait_for_exit(pid, seconds):
    """True once process `pid` has ended; the client removes its socket before it has finished."""
    deadline = time.monotonic() + seconds
    while process_alive(pid):
        if time.monotonic() > deadline:
            return False
        time.sleep(POLL_SECONDS)
    return True


# ---------------------------------------------------------------------------------------------
# Edit scripts


def load_json_file(path, what):
    try:
        text = sys.stdin.read() if path == STDIN_NAME else Path(path).read_text(encoding='utf-8')
    except OSError as error:
        raise MapCtlError('cannot read the %s %s: %s' % (what, path, error))
    try:
        return json.loads(text)
    except ValueError as error:
        raise MapCtlError('the %s %s is not valid JSON: %s' % (what, path, error))


def apply_request(script, script_path, dry_run):
    """map-apply with the script inline, or by path when it is too big for one request line."""
    request = {'cmd': 'map-apply', 'script': script}
    if dry_run:
        request['dry_run'] = True
    try:
        encode_request(request, 0)
        return request
    except MapCtlError:
        if script_path == STDIN_NAME:
            raise MapCtlError('the script is too big for one request; save it to a file and give its path')
    request.pop('script')
    request['path'] = os.path.abspath(script_path)
    return request


# ---------------------------------------------------------------------------------------------
# Request builders: argparse namespace -> request objects (no socket involved)


def absolute(path):
    return os.path.abspath(os.path.expanduser(path))


def map_reference(args, prefix=''):
    """{"map": N} or {"world": N} from --<prefix>map / --<prefix>world, or None."""
    map_number = getattr(args, prefix + 'map', None)
    world = getattr(args, prefix + 'world', None)
    if map_number is not None:
        return {'map': map_number}
    if world is not None:
        return {'world': world}
    return None


def number_or_name(text):
    """A slot, value or direction given as a number or a name."""
    try:
        return int(text)
    except ValueError:
        return text


def build_info(args):
    request = {'cmd': 'map-info'}
    if args.models:
        request['models'] = True
    return [request]


def build_open(args):
    world = args.world if args.world is not None else args.map + 1
    return [{'cmd': 'map-open', 'world': world}]


def build_camera(args):
    if args.topdown is not None:
        if any(value is not None for value in (args.yaw, args.pitch, args.distance, args.height)):
            raise MapCtlError('--yaw, --pitch, --distance and --height go with --tile, not --topdown')
        return [{'cmd': 'map-camera', 'topdown': {'rect': args.topdown}}]
    request = {'cmd': 'map-camera', 'tile': args.tile}
    for key in ('yaw', 'pitch', 'distance', 'height'):
        if getattr(args, key) is not None:
            request[key] = getattr(args, key)
    return [request]


def build_shot(args):
    path = absolute(args.out)
    if not path.lower().endswith(PNG_SUFFIX):
        raise MapCtlError('shot writes PNG: give a path ending in .png')
    requests = []
    if args.topdown is not None:
        requests.append({'cmd': 'map-camera', 'topdown': {'rect': args.topdown}})
    screenshot = {'cmd': 'screenshot', 'out': path, 'clean': not args.overlay}
    region = args.region if args.region is not None else args.topdown
    if region is not None:
        screenshot['region'] = region
    return requests + [screenshot]


def build_export(args):
    request = {'cmd': 'map-export'}
    if args.layers:
        request['layers'] = list(dict.fromkeys(args.layers))
    if args.rect is not None:
        request['rect'] = args.rect
    if args.out is not None:
        request['out'] = absolute(args.out)
    return [request]


def build_query(args):
    if args.rect is not None:
        return [{'cmd': 'map-query', 'rect': args.rect}]
    return [{'cmd': 'map-query', 'tile': args.tile}]


def build_apply(args):
    script = load_json_file(args.script, 'script')
    return [apply_request(script, args.script, args.dry_run)]


def layers_value(layers):
    return layers[0] if len(layers) == 1 else list(dict.fromkeys(layers))


def build_save(args):
    request = {'cmd': 'map-save'}
    if args.layers:
        request['layers'] = layers_value(args.layers)
    return [request]


def build_revert(args):
    return [{'cmd': 'map-revert', 'layers': layers_value(args.layers)}]


def blank_source(args):
    blank = {}
    if args.height is not None:
        blank['height'] = args.height
    if args.texture is not None:
        blank['texture'] = number_or_name(args.texture)
    if args.attribute is not None:
        blank['attribute'] = number_or_name(args.attribute)
    if args.light is not None:
        if len(args.light) not in (1, 3):
            raise MapCtlError('--light is one value or three (red green blue), each 0 to 1')
        blank['light'] = args.light[0] if len(args.light) == 1 else args.light
    textures = map_reference(args, 'textures_from_')
    if textures is not None:
        blank['textures_from'] = textures
    return {'blank': blank}



def build_new_map(args):
    request = dict(map_reference(args), cmd='map-new', name=args.name)
    template = map_reference(args, 'template_')
    if template is not None:
        if any(getattr(args, option) is not None for option in BLANK_OPTIONS):
            raise MapCtlError('--height, --texture, --attribute, --light and --textures-from-* go with --blank')
        request['from'] = {'template': template}
    else:
        request['from'] = blank_source(args)
    models = map_reference(args, 'models_from_')
    if models is not None:
        request['models_from'] = models
    if args.no_minimap:
        request['minimap'] = False
    if args.dry_run:
        request['dry_run'] = True
    return [request]


def build_gates(args):
    return [dict(map_reference(args) or {}, cmd='gate-list')]


def gate_end(args, prefix, with_direction):
    end = dict(map_reference(args, prefix))
    rect = getattr(args, prefix + 'rect')
    if rect is not None:
        end['rect'] = rect
    else:
        end['tile'] = getattr(args, prefix + 'tile')
    if with_direction and args.dir is not None:
        end['dir'] = number_or_name(args.dir)
    return end


def build_gate_add(args):
    request = {'cmd': 'gate-add', 'from': gate_end(args, 'from_', False), 'to': gate_end(args, 'to_', True)}
    if args.level is not None:
        request['level'] = args.level
    if args.allow_trap:
        request['allow_trap'] = True
    if args.dry_run:
        request['dry_run'] = True
    return [request]


def build_gate_remove(args):
    request = {'cmd': 'gate-remove', 'number': args.number}
    if args.dry_run:
        request['dry_run'] = True
    return [request]


def build_gate_show(args):
    return [{'cmd': 'gate-show', 'number': args.number, 'look': not args.no_look}]


def build_tab(args):
    return [{'cmd': 'map-tab', 'tab': args.name}]


def build_server_export(args):
    request = dict(map_reference(args) or {}, cmd='map-server-export')
    if args.safezone_map is not None:
        request['safezone_map'] = args.safezone_map
    if args.out is not None:
        request['out'] = absolute(args.out)
    return [request]


def build_send(args):
    text = sys.stdin.read() if args.request == STDIN_NAME else args.request
    try:
        request = json.loads(text)
    except ValueError as error:
        raise MapCtlError('the request is not valid JSON: %s' % error)
    encode_request(request)
    return [request]


def simple(cmd):
    return lambda args: [{'cmd': cmd}]


# ---------------------------------------------------------------------------------------------
# Running commands


def run_requests(client, requests):
    """Send the requests in order; the last result, with a framing camera's answer as `camera`."""
    results = [result_of(client.request(request)) for request in requests]
    output = dict(results[-1])
    if len(results) > 1:
        output['camera'] = results[0]
    return output


def unsaved_files(info):
    return sorted(name for name, unsaved in (info.get('unsaved') or {}).items() if unsaved)


def run_open(client, args):
    """map-open, unless the loaded map holds unsaved edits it would silently drop."""
    unsaved = unsaved_files(result_of(client.request({'cmd': 'map-info'})))
    if unsaved and not args.discard:
        raise CommandError('the loaded map has unsaved edits (%s) that map-open would drop: save them '
                           '(mapctl.py save) or pass --discard' % ', '.join(unsaved), code='unsaved')
    return run_requests(client, build_open(args))


def run_quit(client, args):
    """quit, then wait for the socket to go and, when the client names its pid, for the process to end."""
    output = dict(result_of(client.request({'cmd': 'quit'})))
    client.close()
    started = time.monotonic()
    output['socket_closed'] = wait_until_gone(client.socket_path, args.wait)
    pid = output.get('pid')
    if pid is None or os.name != 'posix':
        output['exited'] = output['socket_closed']
        return output
    output['exited'] = wait_for_exit(int(pid), max(args.wait - (time.monotonic() - started), 0.0))
    return output


def screenshot_with_retry(client, request):
    """A screenshot, asked for again when its frame could not be read back or another capture ran."""
    for attempt in range(SHOT_RETRIES + 1):
        try:
            return result_of(client.request(request))
        except CommandError as error:
            if error.code not in SHOT_RETRY_CODES or attempt == SHOT_RETRIES:
                raise
            time.sleep(SHOT_RETRY_PAUSE_SECONDS)
    raise MapCtlError('unreachable')


def run_shot(client, args):
    """map-camera (with --topdown), then a clean screenshot that is retried when it fails."""
    requests = build_shot(args)
    camera = [result_of(client.request(request)) for request in requests[:-1]]
    output = dict(screenshot_with_retry(client, requests[-1]))
    if camera:
        output['camera'] = camera[0]
    return output


def run_launch(args):
    timeout = args.timeout if args.timeout is not None else LAUNCH_TIMEOUT_SECONDS
    return launch(args.world, args.socket, args.main, timeout, args.log)


def run_sketch(args):
    rect = args.rect if args.rect is not None else rect_from_legend(args.image)
    try:
        image = map_sketch.read_png(args.image)
        scale = args.scale or map_sketch.auto_scale(image.width, image.height)
        image = image.scaled(scale)
        script = merged_script(args.script) if args.script else None
        drawn = map_sketch.sketch(image, rect, script, args.grid)
        out = absolute(args.out)
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        map_sketch.write_png(image, out)
    except (OSError, map_sketch.SketchError, KeyError, TypeError, ValueError) as error:
        raise MapCtlError('cannot sketch: %s' % (error,))
    return {'out': out, 'size': [image.width, image.height], 'scale': scale, 'rect': rect, 'grid': args.grid,
            'drawn': drawn}


def merged_script(paths):
    """The ops of several scripts as one, numbered on across them in the order given."""
    ops = []
    for path in paths:
        script = load_json_file(path, 'script')
        if not isinstance(script, dict) or not isinstance(script.get('ops'), list):
            raise MapCtlError('the script %s has no "ops" list' % path)
        ops.extend(script['ops'])
    return {'ops': ops}


def rect_from_legend(image_path):
    """The area of a map-export folder's legend.json, for a layer image of that export."""
    legend = Path(image_path).resolve().parent / 'legend.json'
    if not legend.is_file():
        raise MapCtlError('give --rect X0 Y0 X1 Y1, the tiles the image shows (no legend.json next to it)')
    area = load_json_file(str(legend), 'legend').get('area')
    if not (isinstance(area, list) and len(area) == 4):
        raise MapCtlError('%s has no "area"; give --rect' % legend)
    return area


def run_with_client(args):
    timeout = args.timeout if args.timeout is not None else DEFAULT_TIMEOUT_SECONDS
    requests = args.build(args) if args.build is not None else None
    with Client(args.socket, timeout) as client:
        if args.handler is not None:
            return args.handler(client, args)
        return run_requests(client, requests)


def emit(value, compact):
    text = json.dumps(value, separators=(',', ':')) if compact else json.dumps(value, indent=2)
    print(text)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    try:
        args = build_parser().parse_args(argv)
    except UsageError as error:
        emit(error.as_json(), '--compact' in argv)
        print('mapctl: %s' % error, file=sys.stderr)
        return error.exit_code
    try:
        output = args.local(args) if args.local is not None else run_with_client(args)
    except MapCtlError as error:
        emit(error.as_json(), args.compact)
        print('mapctl: %s: %s' % (error.code, error), file=sys.stderr)
        return error.exit_code
    emit(output, args.compact)
    return EXIT_OK


# ---------------------------------------------------------------------------------------------
# Command line


def positive_float(text):
    value = float(text)
    if value <= 0:
        raise argparse.ArgumentTypeError('must be greater than 0')
    return value


def add_tile(parser, name, help_text=None):
    """A tile option; `parser` may be a mutually exclusive group."""
    parser.add_argument(name, nargs=2, type=int, metavar=('X', 'Y'), help=help_text)


def add_rect(parser, name, help_text=None):
    """A tile rectangle option (both corners included); `parser` may be a mutually exclusive group."""
    parser.add_argument(name, nargs=4, type=int, metavar=('X0', 'Y0', 'X1', 'Y1'), help=help_text)


def add_map_choice(parser, prefix='', required=False, what='the map'):
    """--<prefix>map N | --<prefix>world N."""
    group = parser.add_mutually_exclusive_group(required=required)
    flag = '--' + prefix.replace('_', '-')
    group.add_argument(flag + 'map', dest=prefix + 'map', type=int, metavar='N',
                       help='%s, by the game\'s map number (Lorencia 0; new maps 82 to 254)' % what)
    group.add_argument(flag + 'world', dest=prefix + 'world', type=int, metavar='N',
                       help='%s, by its Data/World folder (map number + 1)' % what)


def add_area_choice(parser, prefix, required, what):
    group = parser.add_mutually_exclusive_group(required=required)
    flag = '--' + prefix.replace('_', '-')
    group.add_argument(flag + 'rect', dest=prefix + 'rect', nargs=4, type=int, metavar=('X0', 'Y0', 'X1', 'Y1'),
                       help='%s: tiles, both corners included' % what)
    group.add_argument(flag + 'tile', dest=prefix + 'tile', nargs=2, type=int, metavar=('X', 'Y'),
                       help='%s: one tile' % what)


def add_dry_run(parser, what):
    parser.add_argument('--dry-run', action='store_true', help='only report what %s would do' % what)


class CommandTable:
    """Adds subcommands with the options every command shares."""

    def __init__(self, subparsers, common):
        self.subparsers = subparsers
        self.common = common

    def add(self, name, help_text, build=None, handler=None, local=None):
        parser = self.subparsers.add_parser(name, parents=[self.common], help=help_text, description=help_text)
        parser.set_defaults(build=build, handler=handler, local=local)
        return parser


def common_options(parser, suppress):
    default = argparse.SUPPRESS if suppress else None
    parser.add_argument('--socket', default=default, metavar='PATH',
                        help='control socket (default: $%s, else %s)' % (SOCKET_ENVIRONMENT, default_socket_path()))
    parser.add_argument('--timeout', type=positive_float, default=default, metavar='SECONDS',
                        help='wait this long for an answer (default %g; launch %g)'
                             % (DEFAULT_TIMEOUT_SECONDS, LAUNCH_TIMEOUT_SECONDS))
    parser.add_argument('--compact', action='store_true', default=argparse.SUPPRESS if suppress else False,
                        help='print JSON on one line')


def add_session_commands(table):
    parser = table.add('launch', 'start the editor build offline on a map, with the control socket, and wait '
                       'until it answers; prints its pid', local=run_launch)
    parser.add_argument('--world', type=int, required=True, metavar='N',
                        help='the Data/World folder to open (Lorencia 1; map 82 is 83)')
    parser.add_argument('--main', metavar='PATH', help='the Main executable (default: the first editor build '
                        'found under out/build, Release first)')
    parser.add_argument('--log', metavar='PATH', help='where the client\'s stdout goes (default out/mapctl/)')
    table.add('ping', 'build identifier and scene', build=simple('ping'))
    parser = table.add('info', 'map-info: the loaded map, objects, gates, unsaved files', build=build_info)
    parser.add_argument('--models', action='store_true',
                        help='also list every model the map has, with its type and names')
    parser = table.add('open', 'map-open: switch the offline session to another map (refused while the loaded '
                       'map has unsaved edits)', handler=run_open)
    add_map_choice(parser, required=True)
    parser.add_argument('--discard', action='store_true', help='open it even if unsaved edits are dropped')
    parser = table.add('quit', 'close the client and wait until it has gone', handler=run_quit)
    parser.add_argument('--wait', type=positive_float, default=QUIT_WAIT_SECONDS, metavar='SECONDS',
                        help='how long to wait for the client to exit (default %g)' % QUIT_WAIT_SECONDS)
    parser = table.add('send', 'send any request object, e.g. \'{"cmd": "map-history"}\' (- reads stdin)',
                       build=build_send)
    parser.add_argument('request', metavar='JSON')


def add_view_commands(table):
    parser = table.add('camera', 'map-camera: look at a tile, or straight down on a rectangle', build=build_camera)
    group = parser.add_mutually_exclusive_group(required=True)
    add_tile(group, '--tile', 'the tile to look at')
    add_rect(group, '--topdown', 'look straight down on this rectangle, north up')
    parser.add_argument('--yaw', type=float, help='compass heading of the view: 0 north, 90 east')
    parser.add_argument('--pitch', type=float, help='how far the camera looks down, 5 to 89.7 degrees')
    distance = parser.add_mutually_exclusive_group()
    distance.add_argument('--distance', type=float, help='from the camera to the tile, world units')
    distance.add_argument('--height', type=float, help='above the tile, world units')
    parser = table.add('shot', 'screenshot to a PNG: clean (no editor, HUD or cursor) unless --overlay; asked '
                       'for again when a frame could not be read back', build=build_shot, handler=run_shot)
    parser.add_argument('out', metavar='OUT.png')
    parser.add_argument('--overlay', action='store_true', help='keep the editor panels in the picture')
    add_rect(parser, '--region', 'crop to where these tiles are in the view')
    add_rect(parser, '--topdown', 'first frame these tiles top-down, then shoot them (region = the rectangle)')
    parser = table.add('export', 'map-export: layer PNGs (one pixel per tile), legend.json, objects.json',
                       build=build_export)
    parser.add_argument('--out', metavar='FOLDER', help='default <repo>/out/map-exports/World{N}-<time>/')
    parser.add_argument('--layers', nargs='+', choices=EXPORT_LAYERS, help='default: all')
    add_rect(parser, '--rect', 'only these tiles (default: the whole map)')
    parser = table.add('query', 'map-query: heights, walkability, textures and objects of an area',
                       build=build_query)
    group = parser.add_mutually_exclusive_group(required=True)
    add_rect(group, '--rect', 'an area')
    add_tile(group, '--tile', 'one tile')
    parser = table.add('sketch', 'draw a tile grid and a script\'s shapes on a top-down PNG (no client needed)',
                       local=run_sketch)
    parser.add_argument('image', metavar='IMAGE.png', help='a map-export layer or a top-down region shot')
    parser.add_argument('--out', required=True, metavar='OUT.png')
    add_rect(parser, '--rect', 'the tiles the image shows (default: legend.json next to an export image)')
    parser.add_argument('--script', action='append', metavar='SCRIPT.json',
                        help='an edit script whose shapes to draw; repeat it for several (ops numbered on)')
    parser.add_argument('--scale', type=int, help='enlarge by this factor (default: to about 1024 pixels)')
    parser.add_argument('--grid', type=int, default=map_sketch.DEFAULT_GRID_TILES, metavar='TILES',
                        help='a labelled grid line every this many tiles (0: none; default %d)'
                             % map_sketch.DEFAULT_GRID_TILES)


def add_edit_commands(table):
    parser = table.add('apply', 'map-apply: run an edit script (mu-map-edit/1) as one undo step', build=build_apply)
    parser.add_argument('script', metavar='SCRIPT.json', help='the script file (- reads stdin)')
    add_dry_run(parser, 'the script')
    parser = table.add('dry-run', 'map-apply with dry_run: what the script would change', build=build_apply)
    parser.add_argument('script', metavar='SCRIPT.json', help='the script file (- reads stdin)')
    parser.set_defaults(dry_run=True)
    table.add('undo', 'map-undo: one step back in the Map Editor\'s history', build=simple('map-undo'))
    table.add('redo', 'map-redo: one step forward', build=simple('map-redo'))
    table.add('history', 'map-history: the steps that can be undone and redone', build=simple('map-history'))
    parser = table.add('save', 'map-save: write the map\'s files (default: the ones with unsaved edits)',
                       build=build_save)
    parser.add_argument('layers', nargs='*', choices=SAVE_CHOICES, metavar='FILE',
                        help='all, or some of: ' + ', '.join(MAP_FILES))
    parser = table.add('revert', 'map-revert: read files back from disk, dropping edits and the history',
                       build=build_revert)
    parser.add_argument('layers', nargs='+', choices=SAVE_CHOICES, metavar='FILE',
                        help='all, or some of: ' + ', '.join(MAP_FILES))


def add_new_map_command(table):
    parser = table.add('new-map', 'map-new: a new map (82 to 254), flat or a copy of another', build=build_new_map)
    add_map_choice(parser, required=True, what='the new map')
    parser.add_argument('--name', required=True, help='the name the game shows')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--blank', action='store_true', help='flat ground (see --height ... --textures-from-*)')
    source.add_argument('--template-map', dest='template_map', type=int, metavar='N', help='copy this map')
    source.add_argument('--template-world', dest='template_world', type=int, metavar='N', help='copy this folder')
    parser.add_argument('--height', type=float, help='flat: ground height, 0 to 382.5')
    parser.add_argument('--texture', help='flat: tile slot number or name, e.g. TileGrass01')
    parser.add_argument('--attribute', help='flat: walkable, safezone, blocked, void or water')
    parser.add_argument('--light', nargs='+', type=float, metavar='VALUE', help='flat: 0 to 1, or red green blue')
    add_map_choice(parser, 'textures_from_', what='flat: the map whose tile set is copied')
    add_map_choice(parser, 'models_from_', what='the map whose models are copied')
    parser.add_argument('--no-minimap', action='store_true', help='copy: leave the minimap out')
    add_dry_run(parser, 'map-new')


def add_gate_commands(table):
    parser = table.add('gates', 'gate-list: the gates on a map (default: the loaded one) and the ways in',
                       build=build_gates)
    add_map_choice(parser)
    parser = table.add('gate-add', 'gate-add: a one-way gate between two maps (saves Gate.bmd at once)',
                       build=build_gate_add)
    add_map_choice(parser, 'from_', required=True, what='the enter gate\'s map')
    add_area_choice(parser, 'from_', True, 'the enter gate\'s area')
    add_map_choice(parser, 'to_', required=True, what='the arrival\'s map')
    add_area_choice(parser, 'to_', True, 'the arrival area')
    parser.add_argument('--dir', help='direction players face on arrival: west, south, east, north, ... or 0-8')
    parser.add_argument('--level', type=int, help='level needed to pass')
    parser.add_argument('--allow-trap', action='store_true',
                        help='add it even when no arrival tile is walkable or the arrival overlaps its own enter '
                             'area (refused otherwise: players would be stuck)')
    add_dry_run(parser, 'gate-add')
    parser = table.add('gate-remove', 'gate-remove: remove a gate the editor added (345 and up)',
                       build=build_gate_remove)
    parser.add_argument('number', type=int)
    add_dry_run(parser, 'gate-remove')
    parser = table.add('gate-show', 'gate-show: open the Gates tab on a gate of the loaded map', build=build_gate_show)
    parser.add_argument('number', type=int)
    parser.add_argument('--no-look', action='store_true', help='do not point the camera at it')
    parser = table.add('tab', 'map-tab: switch the Map Editor to a tab (texture leaves the Gates tab\'s ground '
                       'overlay behind)', build=build_tab)
    parser.add_argument('name', metavar='TAB', help='texture, objects, height, attribute, light, gates, '
                                                    '"t. browse", minimap, "o. browse" or assets')
    parser = table.add('server-export', 'map-server-export: the OpenMU files for a map (never applied)',
                       build=build_server_export)
    add_map_choice(parser)
    parser.add_argument('--safezone-map', dest='safezone_map', type=int, metavar='N',
                        help='where players who die there return (default: automatic)')
    parser.add_argument('--out', metavar='FOLDER', help='default <repo>/out/openmu-export/map{N}/')


def build_parser():
    parser = ArgumentParser(prog='mapctl.py', description=__doc__.split('\n\n')[0],
                                     epilog='Exit codes: 0 ok, 1 refused by the client, 2 bad arguments or files, '
                                            '3 no client or no answer. Guide: docs/agents/AI_MAP_EDITING.md')
    common_options(parser, suppress=False)
    common = ArgumentParser(add_help=False)
    common_options(common, suppress=True)
    table = CommandTable(parser.add_subparsers(dest='command', required=True, metavar='COMMAND'), common)
    add_session_commands(table)
    add_view_commands(table)
    add_edit_commands(table)
    add_new_map_command(table)
    add_gate_commands(table)
    return parser


if __name__ == '__main__':
    sys.exit(main())
