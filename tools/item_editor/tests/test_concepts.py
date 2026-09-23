"""Item concept tool (concepts.py and its modules): selection, prompts, cost and caps, multipart,
retries against a local mock server, key redaction and pick. No test talks to OpenAI."""

from email.parser import BytesParser
from email.policy import HTTP
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest import mock
import base64
import contextlib
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import threading
import unittest
import zlib

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import concept_batch  # noqa: E402
import concept_cost  # noqa: E402
import concept_output  # noqa: E402
import concept_prompt  # noqa: E402
import concept_select  # noqa: E402
import concepts  # noqa: E402
import openai_images  # noqa: E402

FAKE_KEY = 'sk-test-DoNotLeakThisKey0123456789'


def png_bytes(width, height, rgb=(128, 128, 128)):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    row = b'\0' + bytes(rgb) * width
    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(row * height))
            + chunk(b'IEND', b''))


def settings_args(**overrides):
    values = dict(preset=None, model=None, quality=None, size=None, variants=None, hints=None, ref_size=None,
                  sheet=False, background=None)
    values.update(overrides)
    return mock.Mock(**values)


ITEMS = concept_select.load_catalog()
PRICES = concept_cost.load_prices()
SECTIONS = concept_prompt.read_sections()
STYLE = concept_prompt.load_style()


class Selection(unittest.TestCase):
    def test_keys_and_tiers_parse(self):
        self.assertEqual(concept_select.parse_keys('0-19, 6-1'), ['0-19', '6-1'])
        self.assertRaises(concept_select.SelectionError, concept_select.parse_keys, '0-19,sword')
        self.assertEqual(concept_select.parse_tiers('T1-T3'), {1, 2, 3})
        self.assertEqual(concept_select.parse_tiers('2,T5'), {2, 5})
        self.assertRaises(concept_select.SelectionError, concept_select.parse_tiers, 'T0-T9')

    def test_study_top_maps_model_files_to_catalog_keys(self):
        subjects = concept_select.select(ITEMS, study_top=10)
        self.assertEqual([s['key'] for s in subjects],
                         ['2-1', '6-0', '12-0', '4-0', '1-0', '3-2', '5-1', '2-0', '0-2', '15-0'])
        books = subjects[-1]
        self.assertEqual(books['kind'], concept_select.KIND_SHARED_GROUP)
        self.assertIn('15-18', books['keys'])
        self.assertEqual(subjects[2]['family'], 'wings')

    def test_an_armour_part_selects_its_whole_set_once(self):
        subjects = concept_select.select(ITEMS, keys=['7-1', '10-1'])
        self.assertEqual(len(subjects), 1)
        self.assertEqual(subjects[0]['key'], '8-1')
        self.assertEqual(subjects[0]['keys'], ['7-1', '8-1', '9-1', '10-1', '11-1'])
        self.assertEqual(len(subjects[0]['models']), 5)

    def test_family_and_tier_filter(self):
        subjects = concept_select.select(ITEMS, families=['swords'], tiers={1})
        self.assertTrue(subjects)
        self.assertTrue(all(s['family'] == 'swords' and s['tier'] == 1 for s in subjects))
        self.assertRaises(concept_select.SelectionError, concept_select.select, ITEMS, families=['lances'])
        self.assertRaises(concept_select.SelectionError, concept_select.select, ITEMS)


class Prompts(unittest.TestCase):
    def subject(self, key):
        return concept_select.select(ITEMS, keys=[key])[0]

    def test_prompt_has_item_tier_family_view_and_note(self):
        prompt = concept_prompt.build_prompt(SECTIONS, STYLE, self.subject('0-2'), concept_prompt.VIEW_SINGLE,
                                             concept_prompt.HINTS_SAME, note='thinner guard')
        self.assertIn('"Rapier", a sword, tier 1 of 7 (Common / grounded)', prompt)
        self.assertIn('#858B8D', prompt)
        self.assertIn('Sword: clear blade', prompt)
        self.assertIn('single orthographic three-quarter view', prompt)
        self.assertIn("Owner's note: thinner guard", prompt)
        self.assertNotIn('$', prompt)

    def test_sheet_view_and_distinct_hints(self):
        subject = self.subject('6-0')
        prompts = [concept_prompt.build_prompt(SECTIONS, STYLE, subject, concept_prompt.VIEW_SHEET,
                                               concept_prompt.HINTS_DISTINCT, variant) for variant in (1, 2, 3)]
        self.assertTrue(all('front view on the left and the side view on the right' in p for p in prompts))
        self.assertIn('faithful refresh', prompts[0])
        self.assertIn('stronger silhouette', prompts[1])
        self.assertIn('more ornate within the tier', prompts[2])
        self.assertNotIn("Owner's note", prompts[0])

    def test_unknown_family_uses_default_and_bad_placeholder_fails(self):
        subject = dict(self.subject('0-2'), family='potions')
        prompt = concept_prompt.build_prompt(SECTIONS, STYLE, subject, 'single', 'same')
        self.assertIn(SECTIONS['family default'], prompt)
        broken = dict(SECTIONS, base='Item $nmae')
        self.assertRaises(concept_prompt.PromptError, concept_prompt.build_prompt, broken, STYLE, subject,
                          'single', 'same')


class Costs(unittest.TestCase):
    def test_per_image_price_and_token_estimate(self):
        estimate = concept_cost.estimate_request(PRICES, 'gpt-image-2', '1024x1024', 'medium', 3, 0, 0, 1024)
        self.assertAlmostEqual(estimate['parts']['output'], 3 * 0.053)
        self.assertEqual(estimate['flags'], [])
        flare = concept_cost.estimate_request(PRICES, 'gpt-image-2.5-flare', '1024x1024', 'medium', 1, 0, 0, 1024)
        self.assertAlmostEqual(flare['parts']['output'], 439 * 30 / 1e6)
        self.assertIn(concept_cost.FLAG_TOKEN_ESTIMATE, flare['flags'])

    def test_reference_and_text_parts_and_size_scaling(self):
        full = concept_cost.estimate_request(PRICES, 'gpt-image-2.5-flare', '1024x1024', 'low', 1, 400, 1, 1024)
        half = concept_cost.estimate_request(PRICES, 'gpt-image-2.5-flare', '1024x1024', 'low', 1, 400, 1, 512)
        self.assertAlmostEqual(full['parts']['reference'], 4354 * 8 / 1e6)
        self.assertAlmostEqual(half['parts']['reference'], full['parts']['reference'] / 4)
        self.assertAlmostEqual(full['parts']['text'], 100 * 5 / 1e6)
        wide = concept_cost.estimate_request(PRICES, 'gpt-image-2', '1536x1024', 'high', 1, 0, 0, 1024)
        self.assertAlmostEqual(wide['parts']['output'], 0.165)
        self.assertIn(concept_cost.FLAG_SIZE_SCALED, wide['flags'])

    def test_quality_and_size_are_checked_per_model(self):
        self.assertRaises(concept_cost.CostError, concept_cost.estimate_request, PRICES, 'gpt-image-2',
                          '1024x1024', 'xhigh', 1, 0, 0, 1024)
        self.assertRaises(concept_cost.CostError, concept_cost.parse_size, '1000x1000')
        self.assertRaises(concept_cost.CostError, concept_cost.model_config, PRICES, 'dall-e-9')

    def test_actual_cost_from_usage_and_caps(self):
        usage = {'input_tokens': 1300, 'output_tokens': 1317,
                 'input_tokens_details': {'image_tokens': 1000, 'text_tokens': 300}}
        actual = concept_cost.actual_cost(PRICES, 'gpt-image-2.5-flare', usage)
        self.assertAlmostEqual(actual['total'], (1000 * 8 + 300 * 5 + 1317 * 30) / 1e6)
        self.assertIsNone(concept_cost.actual_cost(PRICES, 'gpt-image-2', None))
        self.assertEqual(concept_cost.cap_problems(30, 4.99, 30, 5.0), [])
        self.assertEqual(len(concept_cost.cap_problems(31, 5.01, 30, 5.0)), 2)

    def test_presets_and_request_groups(self):
        explore = concept_batch.resolve_settings(settings_args(), PRICES, SECTIONS)
        self.assertEqual((explore['model'], explore['quality'], explore['ref_size']), ('gpt-image-2.5-flare', 'medium', 512))
        self.assertEqual(concept_batch.request_groups(explore), [('same', [1, 2, 3])])
        final = concept_batch.resolve_settings(settings_args(preset='final', sheet=True), PRICES, SECTIONS)
        self.assertEqual((final['model'], final['size'], final['background']), ('gpt-image-2.5-sunburst', '1536x1024', 'opaque'))
        distinct = concept_batch.resolve_settings(settings_args(hints='distinct'), PRICES, SECTIONS)
        self.assertEqual(concept_batch.request_groups(distinct), [(1, [1]), (2, [2]), (3, [3])])
        many = dict(explore, variants=12)
        self.assertEqual([len(v) for _, v in concept_batch.request_groups(many)], [10, 2])
        self.assertRaises(concept_batch.BatchError, concept_batch.resolve_settings,
                          settings_args(hints='distinct', variants=4), PRICES, SECTIONS)

    def test_notes_for_every_item_and_one_item(self):
        notes = concept_batch.parse_notes(['keep it simple', '0-2=thinner guard'])
        subject = {'keys': ['0-2']}
        self.assertEqual(concept_batch.note_for(notes, subject), 'keep it simple; thinner guard')
        self.assertEqual(concept_batch.note_for(notes, {'keys': ['6-0']}), 'keep it simple')


class Multipart(unittest.TestCase):
    def test_fields_and_image_parts(self):
        fields = openai_images.edit_fields('gpt-image-2', 'a sword', '1024x1024', 'low', 'transparent', 3)
        body, content_type = openai_images.encode_multipart(
            fields, [('image[]', 'ref.png', b'\x89PNGdata', 'image/png')], boundary='xyz')
        message = BytesParser(policy=HTTP).parsebytes(b'Content-Type: ' + content_type.encode() + b'\r\n\r\n' + body)
        parts = {part.get_param('name', header='content-disposition'): part for part in message.iter_parts()}
        self.assertEqual(set(parts), {'model', 'prompt', 'size', 'quality', 'background', 'output_format', 'n', 'image[]'})
        self.assertEqual(parts['n'].get_content().strip(), '3')
        self.assertEqual(parts['image[]'].get_filename(), 'ref.png')
        self.assertEqual(parts['image[]'].get_content_type(), 'image/png')
        self.assertEqual(parts['image[]'].get_payload(decode=True), b'\x89PNGdata')
        self.assertNotIn(b'input_fidelity', body)

    def test_limits(self):
        self.assertRaises(openai_images.ApiError, openai_images.edit_fields, 'm', 'p', 's', 'q', 'b', 11)
        client = openai_images.ImagesClient(FAKE_KEY, 'http://127.0.0.1:9')
        self.assertRaises(openai_images.ApiError, client.edit, [], [])


class Redaction(unittest.TestCase):
    def test_headers_and_text(self):
        headers = openai_images.redact_headers({'Authorization': f'Bearer {FAKE_KEY}', 'Content-Type': 'x'})
        self.assertNotIn(FAKE_KEY, json.dumps(headers))
        self.assertEqual(headers['Content-Type'], 'x')
        self.assertNotIn('DoNotLeak', openai_images.scrub(f'Incorrect API key provided: {FAKE_KEY[:12]}***6789'))
        self.assertNotIn(FAKE_KEY, openai_images.scrub(f'x {FAKE_KEY} y', FAKE_KEY))

    def test_key_only_from_env_and_no_plain_http_to_remote_hosts(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertRaises(openai_images.ApiError, openai_images.api_key_from_env)
        with mock.patch.dict(os.environ, {'OPENAI_API_KEY': FAKE_KEY}):
            self.assertEqual(openai_images.api_key_from_env(), FAKE_KEY)
        self.assertRaises(openai_images.ApiError, openai_images.check_api_base, 'http://example.com/v1')
        self.assertEqual(openai_images.check_api_base('http://127.0.0.1:8080/v1/'), 'http://127.0.0.1:8080/v1')


class Backoff(unittest.TestCase):
    def test_retry_after_wins_and_backoff_grows_with_jitter(self):
        self.assertEqual(openai_images.retry_delay(1, 7.0), 7.0)
        self.assertEqual(openai_images.retry_delay(1, 10_000.0), openai_images.RETRY_AFTER_CAP)
        self.assertEqual(openai_images.retry_delay(1, rng=lambda: 0.0), 1.0)
        self.assertEqual(openai_images.retry_delay(3, rng=lambda: 1.0), 8.0)
        self.assertEqual(openai_images.retry_delay(20, rng=lambda: 1.0), openai_images.BACKOFF_CAP)
        self.assertEqual(openai_images.parse_retry_after('3'), 3.0)
        self.assertIsNotNone(openai_images.parse_retry_after('Wed, 21 Oct 2015 07:28:00 GMT'))
        self.assertIsNone(openai_images.parse_retry_after('soon'))


# --- mock server ---------------------------------------------------------------------------------

class MockImages:
    """A local stand-in for POST /v1/images/edits: scripted failures, then the documented shape."""

    def __init__(self, failures=(), images=3):
        self.failures = list(failures)
        self.images = images
        self.requests = []
        self.lock = threading.Lock()
        mock_state = self

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                body = self.rfile.read(int(self.headers['Content-Length']))
                with mock_state.lock:
                    mock_state.requests.append({'path': self.path, 'headers': dict(self.headers), 'body': body})
                    failure = mock_state.failures.pop(0) if mock_state.failures else None
                if failure:
                    return self.reply(*failure)
                return self.reply(200, mock_state.success(body), {'x-request-id': 'req_mock_1'})

            def reply(self, status, payload, headers=None):
                data = json.dumps(payload).encode()
                self.send_response(status)
                for name, value in (headers or {}).items():
                    self.send_header(name, value)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def log_message(self, *args):
                pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.base = f'http://127.0.0.1:{self.server.server_address[1]}/v1'
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def success(self, body):
        count = int(body.split(b'name="n"\r\n\r\n')[1].split(b'\r\n')[0])
        image = base64.b64encode(png_bytes(32, 32, (200, 60, 60))).decode()
        return {'created': 1790000000, 'background': 'transparent', 'output_format': 'png', 'size': '1024x1024',
                'data': [{'b64_json': image} for _ in range(min(count, self.images))],
                'usage': {'input_tokens': 1400, 'output_tokens': 3 * 439, 'total_tokens': 2717,
                          'input_tokens_details': {'image_tokens': 1000, 'text_tokens': 400}}}

    def close(self):
        self.server.shutdown()
        self.server.server_close()


def rate_limited(retry_after='0'):
    return (429, {'error': {'message': 'Rate limit reached', 'code': 'rate_limit_exceeded'}}, {'Retry-After': retry_after})


class ClientAgainstMock(unittest.TestCase):
    def setUp(self):
        self.sleeps = []
        self.records = []

    def client(self, server, attempts=3):
        return openai_images.ImagesClient(FAKE_KEY, server.base, 10, attempts, self.records.append,
                                          sleep=self.sleeps.append)

    def test_429_then_success_honours_retry_after(self):
        server = MockImages([rate_limited('0')])
        self.addCleanup(server.close)
        fields = openai_images.edit_fields('gpt-image-2', 'p', '1024x1024', 'low', 'opaque', 2)
        payload, request_id, attempts = self.client(server).edit(fields, [('r.png', png_bytes(4, 4), 'image/png')])
        self.assertEqual((attempts, request_id, len(payload['data'])), (2, 'req_mock_1', 2))
        self.assertEqual(self.sleeps, [0.0])
        self.assertEqual(server.requests[0]['path'], '/v1/images/edits')
        self.assertEqual(server.requests[0]['headers']['Authorization'], f'Bearer {FAKE_KEY}')
        self.assertNotIn(FAKE_KEY, json.dumps(self.records))

    def test_client_errors_are_not_retried_and_are_scrubbed(self):
        leak = {'error': {'message': f'Incorrect API key provided: {FAKE_KEY}', 'code': 'invalid_api_key'}}
        server = MockImages([(401, leak, {})])
        self.addCleanup(server.close)
        with self.assertRaises(openai_images.ApiError) as caught:
            self.client(server).edit([('n', 1)], [('r.png', b'x', 'image/png')])
        self.assertEqual(caught.exception.status, 401)
        self.assertNotIn(FAKE_KEY, str(caught.exception))
        self.assertEqual((len(server.requests), self.sleeps), (1, []))
        self.assertNotIn(FAKE_KEY, json.dumps(self.records))

    def test_gives_up_after_the_last_attempt(self):
        server = MockImages([(503, {'error': {'message': 'busy'}}, {})] * 3)
        self.addCleanup(server.close)
        with self.assertRaises(openai_images.ApiError):
            self.client(server, attempts=3).edit([('n', 1)], [('r.png', b'x', 'image/png')])
        self.assertEqual((len(server.requests), len(self.sleeps)), (3, 2))


# --- the command line against the mock --------------------------------------------------------

class CommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        self.refs = self.tmp / 'refs'
        self.refs.mkdir()
        (self.refs / '0-2.png').write_bytes(png_bytes(64, 64))

    def main(self, *argv, env=None):
        out = io.StringIO()
        with mock.patch.dict(os.environ, env or {}, clear=True), contextlib.redirect_stdout(out), \
                contextlib.redirect_stderr(out):
            code = concepts.main(list(argv) + ['--out-dir', str(self.tmp), '--refs-dir', str(self.refs)])
        return code, out.getvalue()

    def test_plan_is_the_default_command(self):
        code, out = self.main('--keys', '0-2', '--no-prompts')
        self.assertEqual(code, concepts.EXIT_OK)
        self.assertIn('Total (estimate): 3 images in 1 requests', out)

    def test_run_refuses_over_a_cap_and_spends_nothing_without_yes(self):
        code, out = self.main('run', '--keys', '0-2', '--max-cost', '0.01', '--yes', env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_REFUSED)
        self.assertIn('exceeds --max-cost', out)
        code, out = self.main('run', '--keys', '0-2', '--variants', '31', '--yes')
        self.assertEqual(code, concepts.EXIT_REFUSED)
        code, out = self.main('run', '--keys', '0-2')
        self.assertEqual(code, concepts.EXIT_OK)
        self.assertIn('Nothing was sent', out)
        self.assertEqual(sorted(p.name for p in self.tmp.iterdir()), ['refs'])

    def test_run_needs_a_reference_and_a_key(self):
        code, out = self.main('run', '--keys', '6-0', '--yes', env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_ERROR)
        self.assertIn('no reference image for 6-0', out)
        code, out = self.main('run', '--keys', '0-2', '--yes')
        self.assertEqual(code, concepts.EXIT_ERROR)
        self.assertIn('OPENAI_API_KEY is not set', out)

    def test_full_run_writes_the_batch_and_never_the_key(self):
        server = MockImages([rate_limited('0')])
        self.addCleanup(server.close)
        code, out = self.main('run', '--keys', '0-2', '--model', 'gpt-image-2', '--quality', 'low', '--yes',
                              '--name', 'mock test', '--api-base', server.base, '--verbose',
                              env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertEqual(code, concepts.EXIT_OK, out)
        batch_dir = next(p for p in self.tmp.iterdir() if p.name.endswith('-mock-test'))
        for name in ('v1.png', 'v2.png', 'v3.png', 'v1.prompt.txt', 'v3.meta.json', 'ref.png'):
            self.assertTrue((batch_dir / '0-2' / name).is_file(), name)
        meta = json.loads((batch_dir / '0-2' / 'v2.meta.json').read_text())
        self.assertEqual((meta['model'], meta['quality'], meta['request_id']), ('gpt-image-2', 'low', 'req_mock_1'))
        self.assertEqual(meta['usage']['output_tokens'], 1317)
        self.assertAlmostEqual(meta['actual_cost_request'], (1000 * 8 + 400 * 5 + 1317 * 30) / 1e6, places=6)
        self.assertIn('[redacted]', (batch_dir / 'run.log').read_text())
        self.assertIn('Actual (from usage', out)
        self.assertTrue((batch_dir / 'sheet.html').is_file())
        self.assert_key_absent(out)
        request = server.requests[-1]['body']
        self.assertIn(b'name="image[]"; filename="0-2.png"', request)
        code, out = self.main('run', '--resume', batch_dir.name, '--api-base', server.base, '--yes',
                              env={'OPENAI_API_KEY': FAKE_KEY})
        self.assertIn('Nothing to do', out)
        self.assertEqual(len(server.requests), 2)

    def assert_key_absent(self, output):
        self.assertNotIn(FAKE_KEY, output)
        for path in self.tmp.rglob('*'):
            if path.is_file():
                self.assertNotIn(FAKE_KEY.encode(), path.read_bytes(), path)


@unittest.skipUnless(shutil.which('sips'), 'pick converts with macOS sips')
class Pick(unittest.TestCase):
    def test_pick_writes_a_small_jpeg_prompt_and_meta_and_unpick_removes_them(self):
        tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, tmp)
        variant_dir = tmp / 'batch' / '0-2'
        variant_dir.mkdir(parents=True)
        (variant_dir / 'v2.png').write_bytes(png_bytes(1536, 1024))
        (variant_dir / 'v2.prompt.txt').write_text('the prompt\n')
        (variant_dir / 'v2.meta.json').write_text(json.dumps({'model': 'gpt-image-2', 'variant': 'v2'}))
        concepts_dir = tmp / 'concepts'
        target = concept_output.pick(tmp / 'batch', '0-2', '2', concepts_dir)
        image = (target / 'concept.jpg').read_bytes()
        self.assertTrue(image.startswith(b'\xff\xd8'))
        self.assertLessEqual(len(image), 200_000)
        self.assertEqual((target / 'prompt.txt').read_text(), 'the prompt\n')
        meta = json.loads((target / 'meta.json').read_text())
        self.assertEqual((meta['variant'], meta['picked_from']), ('v2', 'batch/0-2/v2.png'))
        self.assertTrue(concept_output.unpick('0-2', concepts_dir))
        self.assertFalse(target.exists())
        self.assertFalse(concept_output.unpick('0-2', concepts_dir))


if __name__ == '__main__':
    unittest.main()
