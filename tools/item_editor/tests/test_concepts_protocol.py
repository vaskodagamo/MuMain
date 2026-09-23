"""The item editor's side of concepts.py: key lookup (injected Keychain runner), repo-root paths,
the JSON / JSON Lines protocol, refine, list / discard, locks and cancellation. Everything runs
against the local mock server of test_concepts; nothing talks to OpenAI or the real Keychain."""

from pathlib import Path
from unittest import mock
import ast
import contextlib
import io
import json
import os
import queue
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import concept_cancel  # noqa: E402
import concept_events  # noqa: E402
import concept_key  # noqa: E402
import concept_lock  # noqa: E402
import concept_paths  # noqa: E402
import concepts  # noqa: E402
import openai_images  # noqa: E402
from test_concepts import FAKE_KEY, MockImages, no_keychain, png_bytes, rate_limited  # noqa: E402

REPO = TOOLS.parents[1]
SCRIPT = TOOLS / 'concepts.py'
CHILD_TIMEOUT_S = 60
DEAD_PID = 2 ** 22 + 12345  # above macOS/Linux pid_max defaults
MOCK_MODEL = ('--model', 'gpt-image-2', '--quality', 'low')


def keychain_with(key):
    calls = []

    def runner(arguments):
        calls.append(list(arguments))
        return mock.Mock(returncode=0, stdout=key + '\n', stderr='')
    return runner, calls


def stop_child(child):
    if child.poll() is None:
        child.kill()
    child.wait()
    for stream in (child.stdout, child.stderr):
        if stream:
            stream.close()


def json_lines(text):
    return [json.loads(line) for line in text.splitlines() if line.strip()]


class Workspace(unittest.TestCase):
    """A temp out-dir with reference renders, and main() with stdout and stderr kept apart."""

    KEYS = ('0-2', '0-3', '0-4')

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.tmp)
        self.refs = self.tmp / 'refs'
        self.refs.mkdir()
        for key in self.KEYS:
            (self.refs / f'{key}.png').write_bytes(png_bytes(64, 64))
        self.concepts_dir = self.tmp / 'concepts'

    def paths(self):
        return ['--out-dir', str(self.tmp), '--refs-dir', str(self.refs), '--concepts-dir', str(self.concepts_dir)]

    def main(self, *argv, env=None, keychain=no_keychain):
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.dict(os.environ, env or {}, clear=True), contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(err), mock.patch.object(concept_key, 'run_security', keychain):
            code = concepts.main(list(argv) + self.paths())
        return code, out.getvalue(), err.getvalue()

    def server(self, failures=(), gate=None):
        server = MockImages(failures, gate=gate)
        self.addCleanup(server.close)
        return server

    def run_batch(self, server, *extra):
        code, out, err = self.main('run', '--keys', '0-2', *MOCK_MODEL, '--yes', '--json-progress',
                                   '--api-base', server.base, *extra, env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_OK, err)
        return json_lines(out)

    def batch_of(self, events):
        return next(e for e in events if e['event'] == 'started')['batch']

    def assert_key_absent(self, *texts):
        for text in texts:
            self.assertNotIn(FAKE_KEY, text)
        for path in self.tmp.rglob('*'):
            if path.is_file():
                self.assertNotIn(FAKE_KEY.encode(), path.read_bytes(), path)


class KeychainLookup(unittest.TestCase):
    def test_environment_wins_and_the_keychain_is_not_asked(self):
        runner, calls = keychain_with('sk-other-key-0000')
        self.assertEqual(concept_key.api_key({'OPENAI_API_KEY': FAKE_KEY}, runner, 'darwin'), FAKE_KEY)
        self.assertEqual(calls, [])

    def test_keychain_is_read_with_an_argument_list(self):
        runner, calls = keychain_with(FAKE_KEY)
        with mock.patch.object(concept_key, 'keychain_user', return_value='lukas'):
            self.assertEqual(concept_key.api_key({}, runner, 'darwin'), FAKE_KEY)
            concept_key.api_key({concept_key.SERVICE_ENV: 'my-service'}, runner, 'darwin')
        self.assertEqual(calls[0], ['find-generic-password', '-a', 'lukas', '-s', 'openai-api-key', '-w'])
        self.assertEqual(calls[1][4], 'my-service')

    def test_missing_key_names_the_add_command_but_no_secret(self):
        with self.assertRaises(concept_key.NoKeyError) as caught:
            concept_key.api_key({}, no_keychain, 'darwin')
        message = str(caught.exception)
        self.assertIn('set OPENAI_API_KEY', message)
        self.assertIn('security add-generic-password -U -a "$USER" -s openai-api-key -w', message)

        def broken(arguments):
            raise FileNotFoundError('/usr/bin/security')
        with self.assertRaises(concept_key.NoKeyError) as caught:
            concept_key.api_key({}, broken, 'darwin')
        self.assertIn('could not run', str(caught.exception))

    def test_hex_output_of_a_stored_newline_is_decoded(self):
        runner, _ = keychain_with((FAKE_KEY + '\n').encode().hex())
        self.assertEqual(concept_key.api_key({}, runner, 'darwin'), FAKE_KEY)

    def test_other_platforms_skip_the_keychain(self):
        runner, calls = keychain_with(FAKE_KEY)
        self.assertRaises(concept_key.NoKeyError, concept_key.api_key, {}, runner, 'linux')
        self.assertEqual(calls, [])

    def test_status_never_asks_for_the_secret(self):
        runner, calls = keychain_with(FAKE_KEY)
        status = concept_key.key_status({}, runner, 'darwin')
        self.assertEqual((status['found'], status['source']), (True, 'keychain'))
        self.assertNotIn('-w', calls[0])
        self.assertNotIn(FAKE_KEY, json.dumps(status))
        self.assertFalse(concept_key.key_status({}, no_keychain, 'darwin')['found'])
        self.assertEqual(concept_key.key_status({'OPENAI_API_KEY': FAKE_KEY}, runner)['source'], 'env')


class RepoRoot(Workspace):
    def test_root_is_found_from_nested_folders_only(self):
        self.assertEqual(concept_paths.find_repo_root(TOOLS / 'tests'), REPO)
        self.assertRaises(concept_paths.PathError, concept_paths.find_repo_root, self.tmp)
        self.assertRaises(concept_paths.PathError, concept_paths.RepoPaths, self.tmp)

    def test_any_working_directory_and_absolute_paths(self):
        result = subprocess.run([sys.executable, str(SCRIPT), 'plan', '--keys', '0-2', '--json', '--no-prompts'],
                                cwd=str(self.tmp), capture_output=True, text=True, timeout=CHILD_TIMEOUT_S,
                                env={'PATH': os.environ['PATH'], 'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(plan['repo_root'], str(REPO))
        self.assertEqual(plan['out_dir'], str(REPO / 'out' / 'item-concepts'))
        self.assertTrue(Path(plan['items'][0]['reference']['path']).is_absolute())

    def test_a_wrong_repo_root_is_a_usage_error_with_a_json_error(self):
        code, out, err = self.main('list', '--json', '--repo-root', str(self.tmp))
        self.assertEqual(code, concepts.EXIT_USAGE)
        record = json.loads(out)
        self.assertEqual((record['ok'], record['exit'], record['protocol']), (False, 2, 1))
        self.assertIn('not a MuMain checkout', err)


class PlanJson(Workspace):
    def test_one_object_with_items_costs_caps_and_refusal_reasons(self):
        code, out, err = self.main('plan', '--keys', '0-2,6-0', '--json', '--max-cost', '0.05')
        self.assertEqual(code, concepts.EXIT_OK)
        plan = json.loads(out)
        self.assertIn('Plan: 2 item(s)', err)
        self.assertEqual(plan['protocol'], 1)
        items = {item['key']: item for item in plan['items']}
        self.assertTrue(items['0-2']['reference']['exists'])
        self.assertFalse(items['6-0']['reference']['exists'])
        self.assertIn('prompt', items['0-2']['requests'][0])
        self.assertEqual(set(items['0-2']['requests'][0]['estimate']['parts']), {'text', 'reference', 'output'})
        self.assertEqual(plan['totals']['images'], 6)
        self.assertEqual(plan['caps'], {'max_images': 30, 'max_cost': 0.05})
        self.assertTrue(plan['would_refuse'])
        codes = {reason['code'] for reason in plan['reasons']}
        self.assertEqual(codes, {'max_cost', 'missing_reference', 'no_api_key'})
        self.assertFalse(plan['api_key']['found'])

    def test_within_caps_with_a_key_would_run(self):
        code, out, _ = self.main('plan', '--keys', '0-2', '--json', '--no-prompts', env={'OPENAI_API_KEY': FAKE_KEY})
        plan = json.loads(out)
        self.assertEqual((plan['would_refuse'], plan['reasons']), (False, []))
        self.assertNotIn('prompt', plan['items'][0]['requests'][0])
        self.assertNotIn(FAKE_KEY, out)


class ProgressEvents(Workspace):
    def test_a_run_streams_started_retry_done_finished(self):
        server = self.server([rate_limited('0')])
        events = self.run_batch(server)
        names = [event['event'] for event in events]
        self.assertEqual(names, ['started', 'request_started', 'retry', 'request_done', 'finished'])
        self.assertTrue(all(event['protocol'] == 1 for event in events))
        started, _, retry, done, finished = events
        self.assertEqual((started['requests'], started['images']), (1, 3))
        self.assertTrue(Path(started['dir']).is_dir())
        self.assertIn('HTTP 429', retry['reason'])
        self.assertEqual([v['variant'] for v in done['variants']], ['v1', 'v2', 'v3'])
        self.assertTrue(all(Path(v['image']).is_file() for v in done['variants']))
        self.assertAlmostEqual(done['actual']['total'], (1000 * 8 + 400 * 5 + 1317 * 30) / 1e6)
        self.assertEqual((finished['done'], finished['failed'], finished['not_started']), (1, 0, 0))
        self.assertIsNotNone(finished['cost']['actual'])
        self.assertTrue(Path(finished['sheet']).is_file())

    def test_failed_requests_exit_4(self):
        server = self.server([(400, {'error': {'message': 'bad prompt'}}, {})])
        code, out, err = self.main('run', '--keys', '0-2', *MOCK_MODEL, '--yes', '--json-progress',
                                   '--api-base', server.base, env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_FAILED)
        events = json_lines(out)
        self.assertEqual([e['event'] for e in events], ['started', 'request_started', 'request_failed', 'finished'])
        self.assertIn('bad prompt', events[2]['error'])
        self.assertEqual(events[-1]['failed'], 1)
        self.assertIn('failed', err)

    def test_dry_run_refusal_and_errors_are_events(self):
        code, out, _ = self.main('run', '--keys', '0-2', '--json-progress')
        self.assertEqual((code, json_lines(out)[-1]['event']), (concepts.EXIT_OK, 'dry_run'))
        code, out, _ = self.main('run', '--keys', '0-2', '--json-progress', '--max-images', '1', '--yes')
        refused = json_lines(out)[-1]
        self.assertEqual((code, refused['event'], refused['reasons'][0]['code']),
                         (concepts.EXIT_REFUSED, 'refused', 'max_images'))
        code, out, _ = self.main('run', '--keys', '6-0', '--json-progress', '--yes')
        self.assertEqual((code, json_lines(out)[-1]['event']), (concepts.EXIT_USAGE, 'error'))
        code, out, _ = self.main('run', '--keys', '0-2', '--json-progress', '--yes')
        error = json_lines(out)[-1]
        self.assertEqual((code, error['event'], error['exit']), (concepts.EXIT_NO_KEY, 'error', 5))

    def test_the_keychain_key_is_used_and_never_leaks(self):
        server = self.server()
        runner, calls = keychain_with(FAKE_KEY)
        code, out, err = self.main('run', '--keys', '0-2', *MOCK_MODEL, '--yes', '--json-progress',
                                   '--api-base', server.base, '--verbose', keychain=runner)
        self.assertEqual(code, concepts.EXIT_OK, err)
        self.assertEqual(server.requests[0]['headers']['Authorization'], f'Bearer {FAKE_KEY}')
        self.assertEqual(len(calls), 1)
        self.assert_key_absent(out, err)


class Refine(Workspace):
    def refine(self, server, *argv):
        code, out, err = self.main('run', *argv, '--yes', '--json-progress', '--api-base', server.base,
                                   env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_OK, err)
        return json_lines(out)

    def test_refine_sends_the_variant_first_and_records_lineage(self):
        server = self.server()
        first = self.batch_of(self.run_batch(server))
        events = self.refine(server, '--from', f'{first}/0-2/v2', '--note', 'thinner guard')
        second = self.batch_of(events)
        body = server.requests[-1]['body']
        self.assertLess(body.index(b'filename="0-2-parent.png"'), body.index(b'filename="0-2.png"'))
        self.assertIn(b'Revise the attached concept: thinner guard', body)
        batch = json.loads((self.tmp / second / 'batch.json').read_text())
        self.assertEqual((batch['kind'], batch['settings']['model'], batch['settings']['quality']),
                         ('refine', 'gpt-image-2', 'low'))
        meta = json.loads((self.tmp / second / '0-2' / 'v1.meta.json').read_text())
        self.assertEqual(meta['parent'], {'batch': first, 'key': '0-2', 'variant': 'v2'})
        self.assertEqual(meta['images'], ['parent.png', 'ref.png'])
        events = self.refine(server, '--from', f'0-2={second}/v3', '--note', '0-2=gold pommel', '--variants', '1')
        third = self.batch_of(events)
        meta = json.loads((self.tmp / third / '0-2' / 'v1.meta.json').read_text())
        self.assertEqual([link['batch'] for link in meta['lineage']], [second, first])

    def test_refine_needs_a_comment_and_no_other_selection(self):
        server = self.server()
        first = self.batch_of(self.run_batch(server))
        code, _, err = self.main('run', '--from', f'{first}/0-2/v1', '--yes')
        self.assertEqual(code, concepts.EXIT_USAGE)
        self.assertIn('needs a comment', err)
        code, _, _ = self.main('run', '--from', f'{first}/0-2/v1', '--note', 'x', '--family', 'swords')
        self.assertEqual(code, concepts.EXIT_USAGE)
        code, _, _ = self.main('run', '--from', f'{first}/0-2/v9', '--note', 'x')
        self.assertEqual(code, concepts.EXIT_USAGE)

    def test_plan_of_a_refine_shows_parent_and_two_images(self):
        server = self.server()
        first = self.batch_of(self.run_batch(server))
        code, out, _ = self.main('plan', '--from', f'{first}/0-2/v1', '--note', 'x', '--json')
        item = json.loads(out)['items'][0]
        self.assertEqual(item['parent']['variant'], 'v1')
        self.assertTrue(item['parent_image']['exists'])
        self.assertEqual(item['requests'][0]['images'], ['parent.png', 'ref.png'])


class Library(Workspace):
    def test_list_discard_and_summary(self):
        server = self.server()
        first = self.batch_of(self.run_batch(server))
        code, out, _ = self.main('discard', first, '0-2', 'v1', '--json')
        self.assertEqual((code, json.loads(out)['changed']), (concepts.EXIT_OK, True))
        self.assertTrue((self.tmp / first / '0-2' / 'v1.png').is_file())
        code, out, _ = self.main('list', '--key', '0-2', '--json')
        listing = json.loads(out)
        self.assertEqual([v['variant'] for v in listing['variants']], ['v1', 'v2', 'v3'])
        self.assertEqual([v['discarded'] for v in listing['variants']], [True, False, False])
        variant = listing['variants'][1]
        for field in ('batch', 'image', 'prompt', 'model', 'quality', 'created', 'cost', 'parent', 'lineage'):
            self.assertIn(field, variant)
        self.assertTrue(Path(variant['image']).is_absolute())
        self.assertAlmostEqual(variant['cost']['image_actual'] * 3, variant['cost']['request_actual'], places=5)
        self.assertEqual(listing['reference']['source'], 'refs')
        self.assertIsNone(listing['pick'])
        self.main('undiscard', first, '0-2', 'v1', '--json')
        code, out, _ = self.main('list', '--json')
        summary = json.loads(out)
        row = next(row for row in summary['items'] if row['key'] == '0-2')
        self.assertEqual((row['variants'], row['discarded'], row['picked']), (3, 0, False))
        self.assertEqual(summary['batches'][0]['running'], False)
        code, _, _ = self.main('discard', first, '0-2', 'v7', '--json')
        self.assertEqual(code, concepts.EXIT_USAGE)

    @unittest.skipUnless(shutil.which('sips'), 'pick converts with macOS sips')
    def test_pick_shows_up_in_the_listing(self):
        server = self.server()
        first = self.batch_of(self.run_batch(server))
        code, out, _ = self.main('pick', first, '0-2', 'v2', '--json')
        self.assertEqual(code, concepts.EXIT_OK)
        pick = json.loads(out)['pick']
        self.assertTrue(Path(pick['image']).is_file())
        self.assertEqual((pick['batch'], pick['variant']), (first, 'v2'))
        listing = json.loads(self.main('list', '--key', '0-2', '--json')[1])
        self.assertEqual([v['picked'] for v in listing['variants']], [False, True, False])
        code, out, _ = self.main('unpick', '0-2', '--json')
        self.assertTrue(json.loads(out)['removed'])


HOLD_LOCK = """
import sys, time
sys.path.insert(0, sys.argv[1])
import concept_lock
concept_lock.FileLock(sys.argv[2], 'test holder').acquire()
print('held', flush=True)
time.sleep(60)
"""


class Locks(Workspace):
    def hold_in_child(self, path):
        child = subprocess.Popen([sys.executable, '-c', HOLD_LOCK, str(TOOLS), str(path)], stdout=subprocess.PIPE,
                                 text=True)
        self.addCleanup(stop_child, child)
        self.assertEqual(child.stdout.readline().strip(), 'held')
        return child

    def test_a_held_lock_is_refused_and_a_dead_holder_is_taken_over(self):
        path = self.tmp / 'x' / concept_lock.LOCK_FILE
        child = self.hold_in_child(path)
        with self.assertRaises(concept_lock.LockBusy) as caught:
            concept_lock.FileLock(path, 'test').acquire()
        self.assertEqual(caught.exception.holder['pid'], child.pid)
        self.assertTrue(concept_lock.is_locked(path))
        child.kill()
        child.wait()
        self.assertEqual(json.loads(path.read_text())['pid'], child.pid)  # stale: its PID is dead
        with concept_lock.FileLock(path, 'test').acquire():
            self.assertEqual(json.loads(path.read_text())['pid'], os.getpid())
        self.assertFalse(concept_lock.is_locked(path))

    def test_a_leftover_file_with_a_dead_pid_is_free(self):
        path = self.tmp / concept_lock.LOCK_FILE
        path.write_text(json.dumps({'pid': DEAD_PID, 'purpose': 'crashed run'}))
        concept_lock.FileLock(path, 'test').acquire().release()

    def test_a_second_run_of_a_locked_batch_is_busy(self):
        server = self.server([(400, {'error': {'message': 'no'}}, {})])
        code, out, _ = self.main('run', '--keys', '0-2', *MOCK_MODEL, '--yes', '--json-progress',
                                 '--api-base', server.base, env={'OPENAI_API_KEY': FAKE_KEY})
        batch = self.batch_of(json_lines(out))
        self.hold_in_child(self.tmp / batch / concept_lock.LOCK_FILE)
        code, out, _ = self.main('run', '--resume', batch, '--yes', '--json-progress', '--api-base', server.base,
                                 env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_BUSY)
        self.assertEqual(json_lines(out)[-1]['exit'], concepts.EXIT_BUSY)
        summary = json.loads(self.main('list', '--json')[1])
        self.assertTrue(summary['batches'][0]['running'])


class Cancellation(Workspace):
    def test_a_retry_wait_ends_at_once_when_cancelled(self):
        server = self.server([rate_limited('30')])
        cancel = concept_cancel.Cancellation()

        def log(record):
            if record.get('event') == 'retry':
                cancel.set()
        client = openai_images.ImagesClient(FAKE_KEY, server.base, 10, 3, log, sleep=cancel.wait,
                                            cancelled=cancel.is_set)
        started = time.monotonic()
        with self.assertRaises(openai_images.RequestCancelled):
            client.edit([('n', 1)], [('r.png', b'x', 'image/png')])
        self.assertLess(time.monotonic() - started, 10)
        self.assertEqual(len(server.requests), 1)

    def wait_for(self, condition):
        deadline = time.monotonic() + CHILD_TIMEOUT_S
        while not condition():
            self.assertLess(time.monotonic(), deadline, 'timed out')
            time.sleep(0.02)

    def test_sigterm_finishes_the_request_in_flight_and_resume_sends_the_rest(self):
        gate = threading.Event()
        server = self.server(gate=gate)
        command = [sys.executable, str(SCRIPT), 'run', '--keys', ','.join(self.KEYS), *MOCK_MODEL, '--yes',
                   '--json-progress', '--concurrency', '1', '--api-base', server.base] + self.paths()
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                 env={'PATH': os.environ['PATH'], 'OPENAI_API_KEY': FAKE_KEY})
        self.addCleanup(stop_child, child)
        lines = queue.Queue()
        threading.Thread(target=lambda: [lines.put(line) for line in child.stdout], daemon=True).start()
        events = [json.loads(lines.get(timeout=CHILD_TIMEOUT_S))]
        while events[-1]['event'] != 'request_started':
            events.append(json.loads(lines.get(timeout=CHILD_TIMEOUT_S)))
        self.wait_for(lambda: server.requests)  # the first request is on the wire, held by the gate
        child.send_signal(signal.SIGTERM)
        gate.set()
        self.assertEqual(child.wait(CHILD_TIMEOUT_S), concepts.EXIT_CANCELLED, child.stderr.read())
        time.sleep(0.2)  # let the reader thread drain the pipe
        while not lines.empty():
            events.append(json.loads(lines.get()))
        cancelled = events[-1]
        self.assertEqual(cancelled['event'], 'cancelled')
        self.assertEqual((cancelled['done'], cancelled['not_started'], cancelled['reason']), (1, 2, 'SIGTERM'))
        self.assertEqual(cancelled['resume'][:2], ['run', '--resume'])
        self.assertEqual(len(server.requests), 1)
        code, out, err = self.main('run', '--resume', cancelled['batch'], '--yes', '--json-progress',
                                   '--api-base', server.base, env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_OK, err)
        self.assertEqual(json_lines(out)[-1]['done'], 2)
        self.assertEqual(len(server.requests), 3)


class ClosedStdout(unittest.TestCase):
    def test_a_closed_pipe_cancels_instead_of_crashing(self):
        class Broken(io.StringIO):
            def write(self, text):
                raise BrokenPipeError()
        closed = []
        reporter = concept_events.Reporter(concept_events.MODE_PROGRESS, 'run', Broken())
        reporter.on_closed(lambda: closed.append(True))
        reporter.event('started')
        reporter.event('request_started')
        self.assertEqual((closed, reporter.closed), ([True], True))


class PythonCompatibility(unittest.TestCase):
    """Finder starts the editor without the shell profile: python3 may be Apple's 3.9."""

    MINIMUM = (3, 9)

    def test_every_tool_module_parses_as_python_3_9(self):
        for path in sorted(TOOLS.glob('*.py')) + sorted((TOOLS / 'tests').glob('*.py')):
            with self.subTest(path.name):
                ast.parse(path.read_text(encoding='utf-8'), str(path), feature_version=self.MINIMUM)


if __name__ == '__main__':
    unittest.main()
