"""Cancellation of a run by SIGTERM / SIGINT; standard library only.

The first signal sets the flag: no new request starts, requests already on the wire finish (they
are paid for), retry waits end at once, the batch is saved and the command exits with 130. A second
signal exits at once (os._exit(130), no further output): images of requests still on the wire are
lost, and a later `run --resume` sends those requests again.
"""

import contextlib
import os
import signal
import threading

SIGNALS = (signal.SIGTERM, signal.SIGINT)
EXIT_CANCELLED = 130


class Cancellation:
    def __init__(self):
        self.event = threading.Event()
        self.signal_name = None

    def is_set(self):
        return self.event.is_set()

    def set(self, reason='cancelled'):
        if not self.event.is_set():
            self.signal_name = reason
        self.event.set()

    def wait(self, seconds):
        """A sleep that ends early when the run is cancelled (the client's retry wait)."""
        self.event.wait(seconds)

    def _handle(self, signum, frame):
        if self.event.is_set():
            os._exit(EXIT_CANCELLED)
        self.set(signal.Signals(signum).name)

    @contextlib.contextmanager
    def signals_installed(self):
        """Route SIGTERM / SIGINT to this flag (only possible in the main thread)."""
        if threading.current_thread() is not threading.main_thread():
            yield self
            return
        previous = {number: signal.signal(number, self._handle) for number in SIGNALS}
        try:
            yield self
        finally:
            for number, handler in previous.items():
                signal.signal(number, handler)
