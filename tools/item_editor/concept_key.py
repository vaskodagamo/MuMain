"""The OpenAI API key: $OPENAI_API_KEY, else (macOS) the login Keychain; standard library only.

An app started from Finder or the Dock does not read ~/.zshrc, so the item editor's child process
usually has no $OPENAI_API_KEY; the Keychain item works there too. The Keychain is read with
`security find-generic-password -a <user> -s <service> -w`, run with an argument list (no shell),
its stdout captured and handed only to the caller that puts the key into the Authorization
header. Nothing here prints, logs or passes the key on a command line; error texts never contain
security's stdout.
"""

import getpass
import os
import re
import subprocess
import sys

API_KEY_ENV = 'OPENAI_API_KEY'
SERVICE_ENV = 'MU_OPENAI_KEYCHAIN_SERVICE'
DEFAULT_SERVICE = 'openai-api-key'
SECURITY = '/usr/bin/security'
MACOS = 'darwin'
# The Keychain may ask the user to allow access; leave them time to answer.
KEYCHAIN_TIMEOUT_S = 120
ITEM_NOT_FOUND_EXIT = 44
SOURCE_ENV = 'env'
SOURCE_KEYCHAIN = 'keychain'
# `security -w` prints the data as hex when it is not plain text (e.g. a stored trailing newline).
HEX_OUTPUT = re.compile(r'^(?:[0-9a-f]{2})+$')


class NoKeyError(RuntimeError):
    pass


def run_security(arguments):
    """Runs /usr/bin/security (no shell); returns the CompletedProcess with stdout captured."""
    return subprocess.run([SECURITY] + list(arguments), capture_output=True, text=True,
                          stdin=subprocess.DEVNULL, timeout=KEYCHAIN_TIMEOUT_S)


def keychain_service(env):
    return env.get(SERVICE_ENV) or DEFAULT_SERVICE


def keychain_user():
    return getpass.getuser()


def lookup_arguments(user, service, with_secret):
    """find-generic-password prints the secret only with -w; without it only the item's attributes."""
    arguments = ['find-generic-password', '-a', user, '-s', service]
    return arguments + ['-w'] if with_secret else arguments


def add_command(service):
    return f'security add-generic-password -U -a "$USER" -s {service} -w'


def missing_key_message(service, reason=None):
    detail = f' (Keychain: {reason})' if reason else ''
    return (f'no API key: set {API_KEY_ENV} or add the Keychain item (service "{service}", account $USER) '
            f'with: {add_command(service)}   (it asks for the key; nothing lands in the shell history){detail}')


def failure_reason(result):
    if result.returncode == ITEM_NOT_FOUND_EXIT:
        return None
    if result.returncode == 0:
        return 'the item is empty'
    # stderr only: security's stdout is where the secret would be.
    return (result.stderr or '').strip() or f'security exited with {result.returncode}'


def secret_from_output(stdout):
    text = (stdout or '').strip()
    if HEX_OUTPUT.match(text):
        try:
            return bytes.fromhex(text).decode('utf-8').strip()
        except UnicodeDecodeError:
            return ''
    return text


def query_keychain(runner, user, service, with_secret):
    """(CompletedProcess or None, reason text or None)."""
    try:
        return runner(lookup_arguments(user, service, with_secret)), None
    except (OSError, subprocess.SubprocessError) as error:
        return None, f'could not run {SECURITY}: {type(error).__name__}'


def api_key(env=None, runner=None, platform=None):
    """The key from the environment, else the Keychain (macOS); raises NoKeyError."""
    env = os.environ if env is None else env
    runner = runner or run_security
    platform = sys.platform if platform is None else platform
    key = env.get(API_KEY_ENV, '').strip()
    if key:
        return key
    service = keychain_service(env)
    if platform != MACOS:
        raise NoKeyError(f'no API key: set {API_KEY_ENV} (the Keychain lookup is macOS only)')
    result, reason = query_keychain(runner, keychain_user(), service, True)
    key = secret_from_output(result.stdout) if result is not None and result.returncode == 0 else ''
    if key:
        return key
    if reason is None and result is not None:
        reason = failure_reason(result)
    raise NoKeyError(missing_key_message(service, reason))


def key_status(env=None, runner=None, platform=None):
    """Whether a key is configured and where, without reading the Keychain secret."""
    env = os.environ if env is None else env
    runner = runner or run_security
    platform = sys.platform if platform is None else platform
    service = keychain_service(env)
    status = {'found': False, 'source': None, 'env': API_KEY_ENV, 'keychain_service': service}
    if env.get(API_KEY_ENV, '').strip():
        return dict(status, found=True, source=SOURCE_ENV)
    if platform != MACOS:
        return status
    result, _ = query_keychain(runner, keychain_user(), service, False)
    if result is not None and result.returncode == 0:
        return dict(status, found=True, source=SOURCE_KEYCHAIN)
    return dict(status, add_command=add_command(service))
