"""Output of concepts.py: human text, or the editor protocol (JSON) plus human text on stderr.

Modes (assets-work/Items/concepts/README.md, "Editor protocol"):
  text      human text on stdout (the default)
  json      one JSON object on stdout at the end (`--json`)
  progress  JSON Lines on stdout, one event per line, flushed at once (`--json-progress`)
In both JSON modes everything human goes to stderr, so stdout carries only protocol records.
Every record passes through openai_images.scrub() (the key, and anything that looks like one).
"""

import contextlib
import datetime
import io
import json
import os
import sys
import threading

import openai_images

PROTOCOL = 1
MODE_TEXT = 'text'
MODE_JSON = 'json'
MODE_PROGRESS = 'progress'


def now_text():
    return datetime.datetime.now().astimezone().isoformat(timespec='seconds')


class Reporter:
    """Writes protocol records to the stdout it was created with; thread-safe."""

    def __init__(self, mode=MODE_TEXT, command=None, stream=None):
        self.mode = mode
        self.command = command
        self._stream = stream or sys.stdout
        self._lock = threading.Lock()
        self._secret = None
        self._on_closed = None
        self.closed = False

    @property
    def is_json(self):
        return self.mode != MODE_TEXT

    def hide_secret(self, secret):
        self._secret = secret

    def on_closed(self, callback):
        """Called once when stdout goes away (the editor quit): the run then cancels."""
        self._on_closed = callback

    @contextlib.contextmanager
    def human_output(self):
        """In the JSON modes print() goes to stderr for the duration."""
        if not self.is_json:
            yield
            return
        with contextlib.redirect_stdout(sys.stderr):
            yield

    def event(self, name, **fields):
        """One JSON Lines event (progress mode only)."""
        if self.mode == MODE_PROGRESS:
            self._write(dict({'protocol': PROTOCOL, 'event': name, 'time': now_text()}, **fields))

    def result(self, fields):
        """The one result object of a --json command."""
        if self.mode == MODE_JSON:
            self._write(dict({'protocol': PROTOCOL, 'command': self.command, 'ok': True}, **fields))

    def error(self, message, exit_code, **fields):
        if self.mode == MODE_PROGRESS:
            self.event('error', message=message, exit=exit_code, **fields)
        elif self.mode == MODE_JSON:
            self._write(dict({'protocol': PROTOCOL, 'command': self.command, 'ok': False, 'error': message,
                              'exit': exit_code}, **fields))

    def _write(self, record):
        line = openai_images.scrub(json.dumps(record, sort_keys=True), self._secret)
        with self._lock:
            if self.closed:
                return
            try:
                self._stream.write(line + '\n')
                self._stream.flush()
            except (BrokenPipeError, ValueError):
                self._close()

    def _close(self):
        self.closed = True
        point_at_devnull(self._stream)
        if self._on_closed:
            self._on_closed()


def point_at_devnull(stream):
    """Python's recipe for a broken stdout pipe: redirect its descriptor so the exit flush succeeds."""
    try:
        descriptor = os.open(os.devnull, os.O_WRONLY)
        os.dup2(descriptor, stream.fileno())
        os.close(descriptor)
    except (OSError, ValueError, io.UnsupportedOperation):
        pass  # not a real file (tests): nothing is flushed at exit
