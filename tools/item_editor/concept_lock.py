"""Locks of the concept tool: flock()ed files that name their holder; standard library only.

  <out-dir>/.lock    global, held for short sections: allocating a new batch folder and taking its
                     batch lock, pick / unpick / discard / undiscard writes
  <batch>/.lock      held by the run writing that batch, for the whole run: a second run (or
                     --resume) of the same batch is refused at once
  <refs-dir>/.lock   held by `refs` while it renders

The operating system drops a flock when its process dies, so a lock file left behind by a crashed
or killed run (its PID is dead) is simply taken over. The file itself stays; while held it holds
{"pid", "purpose", "since"} of the holder, and it is emptied on release.
"""

from pathlib import Path
import fcntl
import json
import os
import time

import concept_events

LOCK_FILE = '.lock'
POLL_INTERVAL_S = 0.05
GLOBAL_WAIT_S = 10.0
LOCK_FILE_MODE = 0o644


class LockBusy(RuntimeError):
    def __init__(self, path, holder):
        pid = (holder or {}).get('pid', '?')
        purpose = (holder or {}).get('purpose', 'another process')
        super().__init__(f'{path} is held by pid {pid} ({purpose})')
        self.path = Path(path)
        self.holder = holder


def read_holder(path):
    """The holder record of a lock file, or None (missing, empty or unreadable)."""
    try:
        text = Path(path).read_text(encoding='utf-8').strip()
        return json.loads(text) if text else None
    except (OSError, ValueError):
        return None


def try_flock(descriptor):
    try:
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except BlockingIOError:
        return False


class FileLock:
    """An exclusive lock on `path`; acquire(wait) polls up to `wait` seconds, else raises LockBusy."""

    def __init__(self, path, purpose):
        self.path = Path(path)
        self.purpose = purpose
        self._descriptor = None

    def acquire(self, wait=0.0):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(self.path, os.O_RDWR | os.O_CREAT, LOCK_FILE_MODE)
        deadline = time.monotonic() + wait
        while not try_flock(descriptor):
            if time.monotonic() >= deadline:
                os.close(descriptor)
                raise LockBusy(self.path, read_holder(self.path))
            time.sleep(POLL_INTERVAL_S)
        self._descriptor = descriptor
        self._write_holder()
        return self

    def _write_holder(self):
        holder = {'pid': os.getpid(), 'purpose': self.purpose, 'since': concept_events.now_text()}
        os.ftruncate(self._descriptor, 0)
        os.lseek(self._descriptor, 0, os.SEEK_SET)
        os.write(self._descriptor, (json.dumps(holder) + '\n').encode('utf-8'))

    def release(self):
        if self._descriptor is None:
            return
        descriptor, self._descriptor = self._descriptor, None
        try:
            os.ftruncate(descriptor, 0)
        finally:
            fcntl.flock(descriptor, fcntl.LOCK_UN)
            os.close(descriptor)

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.release()


def global_lock(out_dir, purpose, wait=GLOBAL_WAIT_S):
    return FileLock(Path(out_dir) / LOCK_FILE, purpose).acquire(wait)


def batch_lock(batch_dir, purpose):
    return FileLock(Path(batch_dir) / LOCK_FILE, purpose).acquire()


def is_locked(path):
    """Whether someone holds the lock file `path` right now (a missing file is free)."""
    try:
        descriptor = os.open(path, os.O_RDWR)
    except FileNotFoundError:
        return False
    try:
        if not try_flock(descriptor):
            return True
        fcntl.flock(descriptor, fcntl.LOCK_UN)
        return False
    finally:
        os.close(descriptor)
