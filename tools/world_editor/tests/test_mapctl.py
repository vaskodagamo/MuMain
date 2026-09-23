#!/usr/bin/env python3
"""Unit tests for tools/world_editor/mapctl.py and map_sketch.py; no game client needed.

Run: python3 tools/world_editor/tests/test_mapctl.py (ctest runs it as world_editor_mapctl).
The socket tests talk to a fake control socket served by a thread of this process.
"""

import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import zlib

TOOL_FOLDER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_FOLDER))

import map_sketch  # noqa: E402
import mapctl  # noqa: E402

HAS_UNIX_SOCKETS = hasattr(socket, 'AF_UNIX')
SHORT_TIMEOUT = 0.4
RED = (255, 60, 60)


def parse(*argv):
    return mapctl.build_parser().parse_args(list(argv))


def requests_for(*argv):
    args = parse(*argv)
    return args.build(args)


def quiet_exit(*argv):
    """The exit code of a usage error (mapctl raises UsageError, exit 2); None when the line parses."""
    with contextlib.redirect_stderr(io.StringIO()):
        try:
            parse(*argv)
        except SystemExit as stop:
            return stop.code
        except mapctl.UsageError as error:
            return error.exit_code
    return None


class TemporaryFolder:
    def __enter__(self):
        self.path = Path(tempfile.mkdtemp(prefix='mapctl'))
        return self.path

    def __exit__(self, *exception):
        shutil.rmtree(self.path, ignore_errors=True)


class FramingTests(unittest.TestCase):
    def test_request_is_one_compact_line_with_the_id(self):
        line = mapctl.encode_request({'cmd': 'map-query', 'tile': [135, 123]}, 7)
        self.assertTrue(line.endswith(b'\n'))
        self.assertEqual(line.count(b'\n'), 1)
        self.assertEqual(json.loads(line), {'cmd': 'map-query', 'tile': [135, 123], 'id': 7})
        self.assertNotIn(b' ', line)

    def test_request_is_ascii_on_the_wire(self):
        line = mapctl.encode_request({'cmd': 'map-new', 'name': 'Lor\u00e9ncia \u2603'})
        line.decode('ascii')
        self.assertEqual(json.loads(line)['name'], 'Lor\u00e9ncia \u2603')

    def test_request_needs_a_command(self):
        for request in ({}, {'cmd': ''}, {'cmd': 5}, ['ping']):
            with self.assertRaises(mapctl.MapCtlError):
                mapctl.encode_request(request)

    def test_a_request_over_the_line_limit_is_refused(self):
        big = {'cmd': 'map-apply', 'script': {'label': 'x' * mapctl.MAX_REQUEST_BYTES}}
        with self.assertRaisesRegex(mapctl.MapCtlError, 'by path'):
            mapctl.encode_request(big)

    def test_line_buffer_joins_and_splits_chunks(self):
        buffer = mapctl.LineBuffer()
        self.assertEqual(buffer.feed(b'{"ok":tr'), [])
        self.assertEqual(buffer.feed(b'ue}\n\n{"ok":false}\n{"o'), [b'{"ok":true}', b'{"ok":false}'])
        self.assertEqual(buffer.feed(b'k":true}\r\n'), [b'{"ok":true}'])

    def test_line_buffer_refuses_an_endless_answer(self):
        buffer = mapctl.LineBuffer(limit=8)
        with self.assertRaises(mapctl.ProtocolError):
            buffer.feed(b'0123456789')

    def test_responses_are_decoded_and_checked(self):
        self.assertEqual(mapctl.decode_response(b'{"ok":true,"result":{}}'), {'ok': True, 'result': {}})
        for line in (b'not json', b'[1]', b'{"result":{}}', b'{"ok":"yes"}', b'\xff'):
            with self.assertRaises(mapctl.ProtocolError):
                mapctl.decode_response(line)

    def test_an_answer_without_id_belongs_to_the_caller(self):
        self.assertTrue(mapctl.response_matches({'ok': False}, 3))
        self.assertTrue(mapctl.response_matches({'ok': True, 'id': 3}, 3))
        self.assertFalse(mapctl.response_matches({'ok': True, 'id': 2}, 3))

    def test_result_of_raises_the_clients_error(self):
        self.assertEqual(mapctl.result_of({'ok': True, 'result': {'a': 1}}), {'a': 1})
        with self.assertRaises(mapctl.CommandError) as caught:
            mapctl.result_of({'ok': False, 'error': 'busy', 'message': 'held', 'result': {'x': 1}})
        self.assertEqual(caught.exception.code, 'busy')
        self.assertEqual(caught.exception.exit_code, mapctl.EXIT_REFUSED)
        self.assertEqual(caught.exception.as_json(),
                         {'ok': False, 'error': 'busy', 'message': 'held', 'result': {'x': 1}})


class ArgumentTests(unittest.TestCase):
    def test_plain_commands(self):
        expected = {'ping': 'ping', 'info': 'map-info', 'undo': 'map-undo', 'redo': 'map-redo',
                    'history': 'map-history'}
        for command, cmd in expected.items():
            self.assertEqual(requests_for(command), [{'cmd': cmd}])
        self.assertEqual(requests_for('info', '--models'), [{'cmd': 'map-info', 'models': True}])
        self.assertEqual(requests_for('tab', 'texture'), [{'cmd': 'map-tab', 'tab': 'texture'}])

    def test_usage_errors_are_json_on_stdout_too(self):
        code, output, errors = run_main('--compact', 'query', '--tile', '1 2')
        self.assertEqual((code, output['ok'], output['error']), (mapctl.EXIT_USAGE, False, 'bad_arguments'))
        self.assertIn('--tile', output['message'])
        self.assertIn('usage', errors)

    def test_open_takes_a_map_or_a_world(self):
        self.assertEqual(mapctl.build_open(parse('open', '--map', '82')), [{'cmd': 'map-open', 'world': 83}])
        self.assertEqual(mapctl.build_open(parse('open', '--world', '1')), [{'cmd': 'map-open', 'world': 1}])
        self.assertEqual(quiet_exit('open'), mapctl.EXIT_USAGE)
        self.assertEqual(quiet_exit('open', '--map', '1', '--world', '2'), mapctl.EXIT_USAGE)

    def test_camera_on_a_tile_or_top_down(self):
        self.assertEqual(requests_for('camera', '--tile', '212', '130', '--yaw', '0', '--pitch', '50',
                                      '--distance', '4200'),
                         [{'cmd': 'map-camera', 'tile': [212, 130], 'yaw': 0.0, 'pitch': 50.0, 'distance': 4200.0}])
        self.assertEqual(requests_for('camera', '--topdown', '0', '0', '255', '255'),
                         [{'cmd': 'map-camera', 'topdown': {'rect': [0, 0, 255, 255]}}])
        with self.assertRaises(mapctl.MapCtlError):
            requests_for('camera', '--topdown', '0', '0', '9', '9', '--yaw', '10')
        self.assertEqual(quiet_exit('camera'), mapctl.EXIT_USAGE)
        self.assertEqual(quiet_exit('camera', '--tile', '1', '2', '--distance', '1', '--height', '2'),
                         mapctl.EXIT_USAGE)

    def test_shot_is_a_clean_png_at_an_absolute_path(self):
        (shot,) = requests_for('shot', 'before.png')
        self.assertEqual(shot, {'cmd': 'screenshot', 'out': os.path.abspath('before.png'), 'clean': True})
        (shot,) = requests_for('shot', '/tmp/a.PNG', '--overlay', '--region', '1', '2', '3', '4')
        self.assertEqual(shot, {'cmd': 'screenshot', 'out': os.path.abspath('/tmp/a.PNG'), 'clean': False,
                                'region': [1, 2, 3, 4]})
        with self.assertRaisesRegex(mapctl.MapCtlError, 'PNG'):
            requests_for('shot', '/tmp/a.jpg')

    def test_shot_top_down_frames_the_rectangle_first(self):
        camera, shot = requests_for('shot', '/tmp/t.png', '--topdown', '195', '110', '235', '150')
        self.assertEqual(camera, {'cmd': 'map-camera', 'topdown': {'rect': [195, 110, 235, 150]}})
        self.assertEqual(shot['region'], [195, 110, 235, 150])
        self.assertTrue(shot['clean'])

    def test_export_and_query(self):
        (export,) = requests_for('export', '--layers', 'height', 'attribute', 'height', '--rect', '1', '2', '3', '4',
                                 '--out', 'exports/town')
        self.assertEqual(export, {'cmd': 'map-export', 'layers': ['height', 'attribute'], 'rect': [1, 2, 3, 4],
                                  'out': os.path.abspath('exports/town')})
        self.assertEqual(requests_for('export'), [{'cmd': 'map-export'}])
        self.assertEqual(quiet_exit('export', '--layers', 'heights'), mapctl.EXIT_USAGE)
        self.assertEqual(requests_for('query', '--tile', '135', '123'), [{'cmd': 'map-query', 'tile': [135, 123]}])
        self.assertEqual(requests_for('query', '--rect', '0', '0', '9', '9'),
                         [{'cmd': 'map-query', 'rect': [0, 0, 9, 9]}])
        self.assertEqual(quiet_exit('query', '--tile', '1', '2', '--rect', '1', '2', '3', '4'), mapctl.EXIT_USAGE)

    def test_save_and_revert_layers(self):
        self.assertEqual(requests_for('save'), [{'cmd': 'map-save'}])
        self.assertEqual(requests_for('save', 'all'), [{'cmd': 'map-save', 'layers': 'all'}])
        self.assertEqual(requests_for('save', 'height', 'objects', 'height'),
                         [{'cmd': 'map-save', 'layers': ['height', 'objects']}])
        self.assertEqual(requests_for('revert', 'light'), [{'cmd': 'map-revert', 'layers': 'light'}])
        self.assertEqual(quiet_exit('revert'), mapctl.EXIT_USAGE)
        self.assertEqual(quiet_exit('save', 'heights'), mapctl.EXIT_USAGE)

    def test_new_flat_map(self):
        (request,) = requests_for('new-map', '--map', '82', '--name', 'Lorencia Outskirts', '--blank', '--height',
                                  '150', '--texture', 'TileGrass01', '--attribute', '0', '--light', '0.85',
                                  '--textures-from-world', '1', '--dry-run')
        self.assertEqual(request, {'cmd': 'map-new', 'map': 82, 'name': 'Lorencia Outskirts', 'dry_run': True,
                                   'from': {'blank': {'height': 150.0, 'texture': 'TileGrass01', 'attribute': 0,
                                                      'light': 0.85, 'textures_from': {'world': 1}}}})
        (request,) = requests_for('new-map', '--world', '84', '--name', 'N', '--blank', '--light', '1', '0.9', '0.8',
                                  '--texture', '3')
        self.assertEqual(request['from'], {'blank': {'texture': 3, 'light': [1.0, 0.9, 0.8]}})
        with self.assertRaisesRegex(mapctl.MapCtlError, 'one value or three'):
            requests_for('new-map', '--map', '82', '--name', 'N', '--blank', '--light', '1', '1')

    def test_new_map_from_a_template(self):
        (request,) = requests_for('new-map', '--map', '83', '--name', 'Copy', '--template-world', '1',
                                  '--models-from-map', '2', '--no-minimap')
        self.assertEqual(request, {'cmd': 'map-new', 'map': 83, 'name': 'Copy', 'from': {'template': {'world': 1}},
                                   'models_from': {'map': 2}, 'minimap': False})
        with self.assertRaisesRegex(mapctl.MapCtlError, '--blank'):
            requests_for('new-map', '--map', '83', '--name', 'Copy', '--template-map', '0', '--height', '10')
        self.assertEqual(quiet_exit('new-map', '--map', '83', '--name', 'N'), mapctl.EXIT_USAGE)
        self.assertEqual(quiet_exit('new-map', '--map', '83', '--blank'), mapctl.EXIT_USAGE)

    def test_gates(self):
        self.assertEqual(requests_for('gates'), [{'cmd': 'gate-list'}])
        self.assertEqual(requests_for('gates', '--world', '83'), [{'cmd': 'gate-list', 'world': 83}])
        (request,) = requests_for('gate-add', '--from-map', '0', '--from-rect', '245', '92', '246', '97', '--to-map',
                                  '82', '--to-tile', '5', '122', '--dir', 'east', '--level', '10', '--dry-run')
        self.assertEqual(request, {'cmd': 'gate-add', 'from': {'map': 0, 'rect': [245, 92, 246, 97]},
                                   'to': {'map': 82, 'tile': [5, 122], 'dir': 'east'}, 'level': 10, 'dry_run': True})
        (request,) = requests_for('gate-add', '--from-world', '83', '--from-tile', '1', '120', '--to-world', '1',
                                  '--to-rect', '240', '93', '242', '96', '--dir', '3')
        self.assertEqual(request['to']['dir'], 3)
        self.assertNotIn('allow_trap', request)
        (request,) = requests_for('gate-add', '--from-map', '0', '--from-tile', '1', '1', '--to-map', '0', '--to-tile',
                                  '1', '1', '--allow-trap')
        self.assertIs(request['allow_trap'], True)
        self.assertEqual(quiet_exit('gate-add', '--from-map', '0', '--to-map', '82', '--to-tile', '1', '1'),
                         mapctl.EXIT_USAGE)
        self.assertEqual(requests_for('gate-remove', '345', '--dry-run'),
                         [{'cmd': 'gate-remove', 'number': 345, 'dry_run': True}])
        self.assertEqual(requests_for('gate-show', '347'), [{'cmd': 'gate-show', 'number': 347, 'look': True}])
        self.assertEqual(requests_for('gate-show', '347', '--no-look')[0]['look'], False)

    def test_server_export(self):
        self.assertEqual(requests_for('server-export'), [{'cmd': 'map-server-export'}])
        self.assertEqual(requests_for('server-export', '--map', '82', '--safezone-map', '0', '--out', '/tmp/x'),
                         [{'cmd': 'map-server-export', 'map': 82, 'safezone_map': 0,
                           'out': os.path.abspath('/tmp/x')}])

    def test_send_passes_any_request(self):
        self.assertEqual(requests_for('send', '{"cmd": "map-history"}'), [{'cmd': 'map-history'}])
        with self.assertRaises(mapctl.MapCtlError):
            requests_for('send', '{"cmd": ')
        with self.assertRaises(mapctl.MapCtlError):
            requests_for('send', '{"no": "cmd"}')

    def test_shared_options_go_before_or_after_the_command(self):
        before = parse('--socket', '/tmp/a.sock', '--timeout', '5', 'info')
        after = parse('info', '--socket', '/tmp/b.sock', '--compact')
        self.assertEqual((before.socket, before.timeout, before.compact), ('/tmp/a.sock', 5.0, False))
        self.assertEqual((after.socket, after.timeout, after.compact), ('/tmp/b.sock', None, True))
        self.assertEqual(quiet_exit('info', '--timeout', '0'), mapctl.EXIT_USAGE)

    def test_default_socket_comes_from_the_environment(self):
        saved = {name: os.environ.get(name) for name in (mapctl.SOCKET_ENVIRONMENT, mapctl.RUNTIME_DIR_ENVIRONMENT)}
        try:
            os.environ[mapctl.SOCKET_ENVIRONMENT] = '/tmp/from-env.sock'
            self.assertEqual(mapctl.resolve_socket_path(), os.path.abspath('/tmp/from-env.sock'))
            os.environ.pop(mapctl.SOCKET_ENVIRONMENT)
            self.assertTrue(mapctl.resolve_socket_path().endswith(mapctl.DEFAULT_SOCKET_NAME))
            if os.name == 'posix':
                os.environ.pop(mapctl.RUNTIME_DIR_ENVIRONMENT, None)
                # Not /tmp itself, which every user can write to: a folder of this user's own.
                self.assertEqual(mapctl.default_socket_path(),
                                 '/tmp/mu-%d/%s' % (os.getuid(), mapctl.DEFAULT_SOCKET_NAME))
                os.environ[mapctl.RUNTIME_DIR_ENVIRONMENT] = '/run/user/1000'
                self.assertEqual(mapctl.default_socket_path(), '/run/user/1000/' + mapctl.DEFAULT_SOCKET_NAME)
        finally:
            for name, value in saved.items():
                os.environ.pop(name, None)
                if value is not None:
                    os.environ[name] = value

    @unittest.skipUnless(os.name == 'posix', 'per-user socket folders are a POSIX matter')
    def test_the_private_socket_folder_is_made_for_this_user_only(self):
        saved = os.environ.get(mapctl.RUNTIME_DIR_ENVIRONMENT)
        with TemporaryFolder() as root:
            try:
                os.environ[mapctl.RUNTIME_DIR_ENVIRONMENT] = str(root / 'runtime')
                path = mapctl.default_socket_path()
                mapctl.prepare_socket_folder(path)
                info = os.stat(root / 'runtime')
                self.assertEqual((info.st_uid, info.st_mode & 0o777), (os.getuid(), 0o700))
                os.chmod(root / 'runtime', 0o755)
                with self.assertRaisesRegex(mapctl.MapCtlError, 'only you'):
                    mapctl.prepare_socket_folder(path)
            finally:
                os.environ.pop(mapctl.RUNTIME_DIR_ENVIRONMENT, None)
                if saved is not None:
                    os.environ[mapctl.RUNTIME_DIR_ENVIRONMENT] = saved


class ScriptTests(unittest.TestCase):
    SCRIPT = {'schema': 'mu-map-edit/1', 'ops': [{'op': 'terrain.raise', 'amount': 50,
                                                 'shape': {'type': 'circle', 'center': [10, 10], 'radius': 3}}]}

    def test_a_script_file_goes_inline(self):
        with TemporaryFolder() as folder:
            path = folder / 'plan.json'
            path.write_text(json.dumps(self.SCRIPT))
            self.assertEqual(requests_for('apply', str(path)), [{'cmd': 'map-apply', 'script': self.SCRIPT}])
            self.assertEqual(requests_for('apply', str(path), '--dry-run')[0]['dry_run'], True)
            self.assertEqual(requests_for('dry-run', str(path)),
                             [{'cmd': 'map-apply', 'script': self.SCRIPT, 'dry_run': True}])

    def test_a_big_script_goes_by_path(self):
        big = dict(self.SCRIPT, label='x' * mapctl.MAX_REQUEST_BYTES)
        request = mapctl.apply_request(big, 'plans/big.json', False)
        self.assertEqual(request, {'cmd': 'map-apply', 'path': os.path.abspath('plans/big.json')})
        with self.assertRaises(mapctl.MapCtlError):
            mapctl.apply_request(big, mapctl.STDIN_NAME, False)

    def test_bad_script_files_are_named(self):
        with TemporaryFolder() as folder:
            path = folder / 'broken.json'
            path.write_text('{"schema": "mu-map-edit/1",\n "ops": [}')
            with self.assertRaisesRegex(mapctl.MapCtlError, 'not valid JSON.*line 2'):
                requests_for('apply', str(path))
            with self.assertRaisesRegex(mapctl.MapCtlError, 'cannot read'):
                requests_for('apply', str(folder / 'missing.json'))


class LaunchHelperTests(unittest.TestCase):
    def test_socket_paths_have_a_length_limit(self):
        mapctl.check_socket_path('/tmp/' + 'a' * 98, 'darwin')
        with self.assertRaisesRegex(mapctl.MapCtlError, '103'):
            mapctl.check_socket_path('/tmp/' + 'a' * 99, 'darwin')
        mapctl.check_socket_path('/tmp/' + 'a' * 102, 'linux')
        with self.assertRaises(mapctl.MapCtlError):
            mapctl.check_socket_path('/tmp/' + 'a' * 103, 'linux')

    def test_worlds_are_folder_numbers(self):
        mapctl.check_world(1)
        mapctl.check_world(255)
        for world in (0, 256):
            with self.assertRaises(mapctl.MapCtlError):
                mapctl.check_world(world)

    def test_launch_command(self):
        self.assertEqual(mapctl.launch_command('/b/Main', 83, 'linux'), ['/b/Main', '--editor', '--world', '83'])
        self.assertEqual(mapctl.launch_command('/b/Main', 1, 'darwin')[-2:], ['-ApplePersistenceIgnoreState', 'YES'])

    def test_the_editor_build_is_found_release_first(self):
        with TemporaryFolder() as root:
            build = root / 'out' / 'build' / 'macos-arm64-mueditor' / 'src'
            empty_bundle = build / 'RelWithDebInfo' / 'Main.app' / 'Contents' / 'MacOS'
            empty_bundle.mkdir(parents=True)
            with self.assertRaises(mapctl.MapCtlError) as caught:
                mapctl.find_editor_main(root, 'darwin')
            self.assertEqual(caught.exception.code, 'no_build')
            debug = build / 'Debug' / 'Main.app' / 'Contents' / 'MacOS' / 'Main'
            release = build / 'Release' / 'Main.app' / 'Contents' / 'MacOS' / 'Main'
            for path in (debug, release):
                path.parent.mkdir(parents=True)
                path.write_bytes(b'')
            self.assertEqual(mapctl.find_editor_main(root, 'darwin'), release)
            linux = root / 'out' / 'build' / 'linux-x64-mueditor' / 'src' / 'Debug' / 'Main'
            linux.parent.mkdir(parents=True)
            linux.write_bytes(b'')
            self.assertEqual(mapctl.find_editor_main(root, 'linux'), linux)


class FakeControlSocket:
    """A thread serving a control socket: `answer(request)` returns the response object or None."""

    def __init__(self, answer):
        self.answer = answer
        self.requests = []
        self.folder = tempfile.mkdtemp(prefix='mc', dir='/tmp' if os.path.isdir('/tmp') else None)
        self.path = os.path.join(self.folder, 's.sock')
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.path)
        self.server.listen(4)
        self.server.settimeout(0.1)
        self.running = True
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.running = False
        self.thread.join(2)
        self.server.close()
        shutil.rmtree(self.folder, ignore_errors=True)

    def _serve(self):
        while self.running:
            try:
                connection, _ = self.server.accept()
            except (socket.timeout, OSError):
                continue
            with connection:
                self._serve_connection(connection)

    def _serve_connection(self, connection):
        buffer = mapctl.LineBuffer()
        connection.settimeout(0.1)
        while self.running:
            try:
                data = connection.recv(4096)
            except socket.timeout:
                continue
            if not data:
                return
            for line in buffer.feed(data):
                request = json.loads(line)
                self.requests.append(request)
                self._send(connection, request, self.answer(request))

    @staticmethod
    def _send(connection, request, responses):
        """Each response in two writes, with the request's id unless it names another."""
        if responses is None:
            return
        for response in responses if isinstance(responses, list) else [responses]:
            if 'id' in request and 'id' not in response:
                response = dict(response, id=request['id'])
            encoded = (json.dumps(response) + '\n').encode()
            half = len(encoded) // 2
            connection.sendall(encoded[:half])
            time.sleep(0.01)
            connection.sendall(encoded[half:])


def answers(results):
    """A fake client that answers each command from `results` (a result object or an error tuple)."""
    def answer(request):
        value = results.get(request['cmd'])
        if value is None:
            return None
        if isinstance(value, tuple):
            return {'ok': False, 'error': value[0], 'message': value[1]}
        return {'ok': True, 'result': value}
    return answer


def run_main(*argv):
    output, errors = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
        code = mapctl.main(list(argv))
    return code, json.loads(output.getvalue()), errors.getvalue()


@unittest.skipUnless(HAS_UNIX_SOCKETS, 'this Python has no AF_UNIX sockets')
class ClientTests(unittest.TestCase):
    def test_call_returns_the_result(self):
        with FakeControlSocket(answers({'ping': {'scene': 'world'}})) as fake:
            with mapctl.Client(fake.path, SHORT_TIMEOUT * 5) as client:
                self.assertEqual(client.call('ping'), {'scene': 'world'})
                self.assertEqual(client.call('ping'), {'scene': 'world'})
            self.assertEqual([request['id'] for request in fake.requests], [1, 2])

    def test_a_refusal_raises_the_clients_code(self):
        with FakeControlSocket(answers({'map-undo': ('failed', 'there is nothing to undo')})) as fake:
            with mapctl.Client(fake.path, SHORT_TIMEOUT * 5) as client:
                with self.assertRaises(mapctl.CommandError) as caught:
                    client.call('map-undo')
        self.assertEqual((caught.exception.code, str(caught.exception)), ('failed', 'there is nothing to undo'))

    def test_answers_for_other_requests_are_skipped(self):
        def answer(request):
            return [{'ok': True, 'id': 999, 'result': {'mine': False}}, {'ok': True, 'result': {'mine': True}}]

        with FakeControlSocket(answer) as fake:
            with mapctl.Client(fake.path, SHORT_TIMEOUT * 5) as client:
                self.assertEqual(client.call('ping'), {'mine': True})

    def test_no_answer_times_out(self):
        with FakeControlSocket(answers({})) as fake:
            with mapctl.Client(fake.path, SHORT_TIMEOUT) as client:
                with self.assertRaises(mapctl.NoAnswer) as caught:
                    client.call('map-info')
        self.assertIn('within', str(caught.exception))
        self.assertEqual(caught.exception.exit_code, mapctl.EXIT_UNREACHABLE)

    def test_a_missing_socket_says_how_to_start_one(self):
        with self.assertRaisesRegex(mapctl.Unreachable, 'launch'):
            mapctl.Client('/tmp/mapctl-test-no-such.sock').connect()
        code, output, errors = run_main('--socket', '/tmp/mapctl-test-no-such.sock', 'info')
        self.assertEqual((code, output['ok'], output['error']), (mapctl.EXIT_UNREACHABLE, False, 'unreachable'))
        self.assertIn('unreachable', errors)

    def test_main_prints_the_result_or_the_refusal(self):
        results = {'map-info': {'world': 1, 'name': 'Lorencia'}, 'map-redo': ('failed', 'there is nothing to redo')}
        with FakeControlSocket(answers(results)) as fake:
            self.assertEqual(run_main('--socket', fake.path, 'info')[:2],
                             (mapctl.EXIT_OK, {'world': 1, 'name': 'Lorencia'}))
            code, output, errors = run_main('redo', '--socket', fake.path)
        self.assertEqual(code, mapctl.EXIT_REFUSED)
        self.assertEqual(output, {'ok': False, 'error': 'failed', 'message': 'there is nothing to redo'})
        self.assertIn('nothing to redo', errors)

    def test_a_top_down_shot_frames_then_captures(self):
        results = {'map-camera': {'area': [0, 0, 9, 9]}, 'screenshot': {'path': '/tmp/x.png', 'width': 10}}
        with FakeControlSocket(answers(results)) as fake:
            code, output, _ = run_main('shot', '/tmp/x.png', '--topdown', '0', '0', '9', '9', '--socket', fake.path)
        self.assertEqual(code, mapctl.EXIT_OK)
        self.assertEqual(output, {'path': '/tmp/x.png', 'width': 10, 'camera': {'area': [0, 0, 9, 9]}})
        self.assertEqual([request['cmd'] for request in fake.requests], ['map-camera', 'screenshot'])

    def test_quit_waits_until_the_socket_is_gone(self):
        def answer(request):
            os.remove(fake.path)
            return {'ok': True, 'result': {'quitting': True}}

        fake = FakeControlSocket(answer)
        with fake:
            code, output, _ = run_main('quit', '--socket', fake.path, '--wait', '2')
        self.assertEqual((code, output),
                         (mapctl.EXIT_OK, {'quitting': True, 'socket_closed': True, 'exited': True}))

    @unittest.skipUnless(os.name == 'posix', 'waits for a process id')
    def test_quit_waits_for_the_process_the_client_names(self):
        # The socket goes at once, the process only when it has shut down.
        slow = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(0.8)'])

        def answer(request):
            os.remove(fake.path)
            return {'ok': True, 'result': {'quitting': True, 'pid': slow.pid}}

        fake = FakeControlSocket(answer)
        with fake:
            code, output, _ = run_main('quit', '--socket', fake.path, '--wait', '5')
        self.assertEqual((code, output['socket_closed'], output['exited']), (mapctl.EXIT_OK, True, True))
        self.assertIsNotNone(slow.poll())
        # A process that goes on running (this test's own) is reported as not gone.
        self.assertFalse(mapctl.wait_for_exit(os.getpid(), 0.3))

    def test_a_shot_whose_frame_was_lost_is_taken_again(self):
        attempts = []

        def answer(request):
            attempts.append(request['cmd'])
            if len(attempts) == 1:
                return {'ok': False, 'error': 'failed', 'message': 'the frame could not be read back'}
            return {'ok': True, 'result': {'path': '/tmp/x.png'}}

        saved = mapctl.SHOT_RETRY_PAUSE_SECONDS
        mapctl.SHOT_RETRY_PAUSE_SECONDS = 0.01
        try:
            with FakeControlSocket(answer) as fake:
                code, output, _ = run_main('shot', '/tmp/x.png', '--socket', fake.path)
        finally:
            mapctl.SHOT_RETRY_PAUSE_SECONDS = saved
        self.assertEqual((code, output), (mapctl.EXIT_OK, {'path': '/tmp/x.png'}))
        self.assertEqual(attempts, ['screenshot', 'screenshot'])

    def test_open_keeps_unsaved_edits_unless_told_to_drop_them(self):
        results = {'map-info': {'unsaved': {'height': True, 'light': False, 'objects': True}},
                   'map-open': {'world': 3}}
        with FakeControlSocket(answers(results)) as fake:
            code, output, _ = run_main('open', '--world', '3', '--socket', fake.path)
            self.assertEqual((code, output['error']), (mapctl.EXIT_REFUSED, 'unsaved'))
            self.assertIn('height, objects', output['message'])
            self.assertEqual(run_main('open', '--world', '3', '--discard', '--socket', fake.path)[:2],
                             (mapctl.EXIT_OK, {'world': 3}))
        self.assertEqual([request['cmd'] for request in fake.requests], ['map-info', 'map-info', 'map-open'])

    def test_launch_refuses_a_socket_that_is_served(self):
        with FakeControlSocket(answers({'ping': {'scene': 'world'}})) as fake:
            with self.assertRaises(mapctl.CommandError) as caught:
                mapctl.launch(1, fake.path, main='/nonexistent/Main')
        self.assertEqual(caught.exception.code, 'already_running')

    def test_launch_refuses_a_client_too_busy_to_answer(self):
        saved = mapctl.PING_TIMEOUT_SECONDS
        mapctl.PING_TIMEOUT_SECONDS = SHORT_TIMEOUT
        try:
            with FakeControlSocket(answers({})) as fake:
                with self.assertRaises(mapctl.CommandError) as caught:
                    mapctl.launch(1, fake.path, main='/nonexistent/Main')
        finally:
            mapctl.PING_TIMEOUT_SECONDS = saved
        self.assertEqual(caught.exception.code, 'already_running')
        self.assertIn('busy', str(caught.exception))

    def test_a_ping_from_another_process_is_not_the_launched_client(self):
        self.assertFalse(mapctl.answered_by_other({'scene': 'world'}, 42))
        self.assertFalse(mapctl.answered_by_other({'scene': 'world', 'pid': 42}, 42))
        self.assertTrue(mapctl.answered_by_other({'scene': 'world', 'pid': 7}, 42))


def filtered_png(width, height, channels, rows, filters):
    """A PNG whose rows use the given filter types (the decoder must undo each)."""
    colour_type = {1: 0, 3: 2, 4: 6}[channels]
    raw, previous = b'', bytes(width * channels)
    for row, kind in zip(rows, filters):
        encoded = bytearray()
        for i, value in enumerate(row):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            up_left = previous[i - channels] if i >= channels else 0
            predictor = {0: 0, 1: left, 2: up, 3: (left + up) >> 1,
                         4: map_sketch._paeth(left, up, up_left)}[kind]
            encoded.append((value - predictor) & 0xFF)
        raw += bytes([kind]) + bytes(encoded)
        previous = row
    header = struct.pack('>IIBBBBB', width, height, 8, colour_type, 0, 0, 0)
    return (map_sketch.PNG_SIGNATURE + map_sketch._chunk(b'IHDR', header) +
            map_sketch._chunk(b'IDAT', zlib.compress(raw)) + map_sketch._chunk(b'IEND', b''))


class SketchTests(unittest.TestCase):
    def test_png_round_trip(self):
        image = map_sketch.RgbImage(3, 2, bytes(range(18)))
        decoded = map_sketch.decode_png(map_sketch.encode_png(image))
        self.assertEqual((decoded.width, decoded.height, bytes(decoded.pixels)), (3, 2, bytes(range(18))))

    def test_every_png_filter_and_colour_type_is_read(self):
        rows = [bytes([10, 200, 30, 40, 50, 60]), bytes([70, 80, 90, 5, 255, 1]), bytes([9, 8, 7, 6, 5, 4]),
                bytes([100, 0, 100, 0, 100, 0]), bytes([1, 2, 3, 250, 251, 252])]
        image = map_sketch.decode_png(filtered_png(2, 5, 3, rows, [0, 1, 2, 3, 4]))
        self.assertEqual(bytes(image.pixels), b''.join(rows))
        grey = map_sketch.decode_png(filtered_png(2, 1, 1, [bytes([7, 250])], [4]))
        self.assertEqual(bytes(grey.pixels), bytes([7, 7, 7, 250, 250, 250]))
        rgba = map_sketch.decode_png(filtered_png(1, 1, 4, [bytes([1, 2, 3, 99])], [1]))
        self.assertEqual(bytes(rgba.pixels), bytes([1, 2, 3]))
        with self.assertRaises(map_sketch.SketchError):
            map_sketch.decode_png(b'GIF89a')

    def test_tiles_map_to_pixels_north_up(self):
        view = map_sketch.TileView([100, 90, 109, 99], 40, 40)
        self.assertEqual(view.to_pixel((100, 100)), (0, 0))
        self.assertEqual(view.to_pixel((110, 90)), (40, 40))
        self.assertEqual(view.to_pixel((100.5, 99.5)), (2, 2))
        self.assertEqual(map_sketch.auto_scale(256, 256), 4)
        self.assertEqual(map_sketch.auto_scale(2000, 1000), 1)
        self.assertEqual(map_sketch.auto_scale(10, 10), map_sketch.MAX_SCALE)

    def test_script_items_follow_the_ops(self):
        road = {'type': 'path', 'points': [[1, 1], [5, 1]], 'width': 2}
        script = {'ops': [
            {'op': 'terrain.raise', 'shape': {'type': 'circle', 'center': [5, 5], 'radius': 2}},
            {'op': 'object.scatter', 'shape': {'type': 'rect', 'rect': [0, 0, 9, 9]}, 'avoid': {'areas': [road]}},
            {'op': 'object.place', 'model': 'Tree01', 'tile': [3, 4]},
            {'op': 'object.delete', 'select': {'inside': {'type': 'polygon', 'points': [[0, 0], [2, 0], [0, 2]]}}},
        ]}
        drawn = [item.describe() for item in map_sketch.script_items(script)]
        self.assertEqual([(entry['op'], entry['draws'], entry['colour']) for entry in drawn],
                         [(0, 'circle', 'yellow'), (1, 'rect', 'cyan'), (1, 'path', 'white'), (2, 'point', 'cyan'),
                          (3, 'polygon', 'green')])
        with self.assertRaises(map_sketch.SketchError):
            map_sketch.script_items({'schema': 'mu-map-edit/1'})

    def test_a_rectangle_is_outlined_on_its_tile_edges(self):
        image = map_sketch.RgbImage(10, 10).scaled(4)
        script = {'ops': [{'op': 'attribute.set', 'value': 'blocked', 'shape': {'type': 'rect', 'rect': [2, 2, 5, 5]}}]}
        drawn = map_sketch.sketch(image, [0, 0, 9, 9], script, grid=0)
        self.assertEqual(drawn, [{'op': 0, 'name': 'attribute.set', 'draws': 'rect', 'colour': 'red'}])
        left_edge_x, row_inside = 2 * 4, (10 - 4) * 4
        self.assertEqual(image.get(left_edge_x, row_inside), RED)
        self.assertEqual(image.get(1, 1), (0, 0, 0))

    def test_the_grid_is_labelled(self):
        image = map_sketch.RgbImage(20, 20).scaled(4)
        map_sketch.sketch(image, [100, 100, 119, 119], None, grid=10)
        grid_x = 10 * 4
        self.assertNotEqual(image.get(grid_x, 5), (0, 0, 0))
        self.assertEqual(image.get(grid_x + 20, 5), (0, 0, 0))

    def test_sketch_command_reads_the_export_legend(self):
        with TemporaryFolder() as folder:
            map_sketch.write_png(map_sketch.RgbImage(16, 16), str(folder / 'height.png'))
            (folder / 'legend.json').write_text(json.dumps({'area': [100, 90, 115, 105]}))
            script = folder / 'plan.json'
            script.write_text(json.dumps({'ops': [{'op': 'light.tint', 'color': [1, 1, 1], 'shape': {
                'type': 'circle', 'center': [108, 98], 'radius': 4}}]}))
            code, output, _ = run_main('sketch', str(folder / 'height.png'), '--script', str(script), '--script',
                                       str(script), '--out', str(folder / 'plan.png'))
            self.assertEqual(code, mapctl.EXIT_OK)
            self.assertEqual((output['rect'], output['scale'], output['size']), ([100, 90, 115, 105], 16, [256, 256]))
            drawn = [(entry['op'], entry['colour']) for entry in output['drawn']]
            self.assertEqual(drawn, [(0, 'orange'), (1, 'orange')])
            self.assertEqual(map_sketch.read_png(str(folder / 'plan.png')).width, 256)
            shot = folder / 'shots' / 'top.png'
            shot.parent.mkdir()
            map_sketch.write_png(map_sketch.RgbImage(8, 8), str(shot))
            code, output, _ = run_main('sketch', str(shot), '--out', str(folder / 'x.png'))
        self.assertEqual(code, mapctl.EXIT_USAGE)
        self.assertIn('--rect', output['message'])


if __name__ == '__main__':
    unittest.main()
