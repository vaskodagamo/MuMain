"""Minimal client for the OpenAI Images API edit endpoint; standard library only.

POST {api_base}/images/edits, multipart/form-data (hand-built): text fields (model, prompt, size,
quality, background, output_format, n, ...) and one `image[]` file part per reference image.
The response carries `data[i].b64_json`, `usage` and the `x-request-id` header.
Docs: https://developers.openai.com/api/reference/resources/images/methods/edit
      https://developers.openai.com/api/docs/guides/image-generation

Security: the API key comes only from the OPENAI_API_KEY environment variable and only ever goes
into the Authorization header. Every text this module hands back (errors, log records) is passed
through scrub(), which removes the key and anything that looks like one; headers are only ever
shown through redact_headers().
"""

from email.utils import parsedate_to_datetime
from urllib.parse import urlsplit
import datetime
import json
import os
import random
import re
import socket
import ssl
import time
import urllib.error
import urllib.request
import uuid

API_KEY_ENV = 'OPENAI_API_KEY'
API_BASE = 'https://api.openai.com/v1'
EDITS_PATH = '/images/edits'
IMAGE_FIELD = 'image[]'
MAX_IMAGES = 16
MAX_N = 10
LOOPBACK_HOSTS = {'127.0.0.1', 'localhost', '::1'}
REQUEST_ID_HEADER = 'x-request-id'
AUTH_HEADER = 'Authorization'
REDACTED = '[redacted]'
KEY_LIKE = re.compile(r'sk-[A-Za-z0-9_\-*.]{3,}')
RETRY_STATUSES = {408, 409, 429, 500, 502, 503, 504}
NO_RETRY_CODES = {'insufficient_quota', 'billing_hard_limit_reached'}
DEFAULT_TIMEOUT = 300.0
DEFAULT_ATTEMPTS = 5
BACKOFF_BASE = 2.0
BACKOFF_CAP = 60.0
RETRY_AFTER_CAP = 300.0
ERROR_TEXT_LIMIT = 500


class ApiError(RuntimeError):
    """A request failed for good (after retries, or not retryable). The message is scrubbed."""

    def __init__(self, message, status=None, request_id=None):
        super().__init__(message)
        self.status = status
        self.request_id = request_id


def api_key_from_env():
    key = os.environ.get(API_KEY_ENV, '').strip()
    if not key:
        raise ApiError(f'{API_KEY_ENV} is not set; export it in your shell (see assets-work/Items/concepts/README.md)')
    return key


def check_api_base(api_base):
    """Only https, or plain http to this machine (tests): the key must never travel in clear text."""
    parts = urlsplit(api_base)
    if parts.scheme == 'https' or (parts.scheme == 'http' and parts.hostname in LOOPBACK_HOSTS):
        return api_base.rstrip('/')
    raise ApiError(f'refusing API base {api_base!r}: use https (or http://127.0.0.1 for tests)')


def scrub(text, key=None):
    text = str(text)
    if key:
        text = text.replace(key, REDACTED)
    return KEY_LIKE.sub(f'sk-{REDACTED}', text)


def redact_headers(headers):
    return {name: (f'Bearer {REDACTED}' if name.lower() == AUTH_HEADER.lower() else value)
            for name, value in headers.items()}


def encode_multipart(fields, files, boundary=None):
    """fields [(name, value)], files [(name, filename, bytes, content type)] -> (body, content type)."""
    boundary = boundary or f'mu-concepts-{uuid.uuid4().hex}'
    lines = []
    for name, value in fields:
        lines += [f'--{boundary}'.encode(), f'Content-Disposition: form-data; name="{name}"'.encode(),
                  b'', str(value).encode('utf-8')]
    for name, filename, data, content_type in files:
        disposition = f'Content-Disposition: form-data; name="{name}"; filename="{filename}"'
        lines += [f'--{boundary}'.encode(), disposition.encode(),
                  f'Content-Type: {content_type}'.encode(), b'', data]
    lines += [f'--{boundary}--'.encode(), b'']
    return b'\r\n'.join(lines), f'multipart/form-data; boundary={boundary}'


def edit_fields(model, prompt, size, quality, background, n, output_format='png', input_fidelity=None):
    """The text fields of an edit request, in the documented names."""
    if not 1 <= n <= MAX_N:
        raise ApiError(f'n must be 1..{MAX_N}, not {n}')
    fields = [('model', model), ('prompt', prompt), ('size', size), ('quality', quality),
              ('background', background), ('output_format', output_format), ('n', n)]
    if input_fidelity:
        fields.append(('input_fidelity', input_fidelity))
    return fields


def parse_retry_after(value, now=None):
    """Retry-After header (seconds or HTTP date) -> seconds, or None."""
    if not value:
        return None
    try:
        return max(0.0, float(value))
    except ValueError:
        pass
    try:
        when = parsedate_to_datetime(value)
    except (TypeError, ValueError):
        return None
    now = now or datetime.datetime.now(datetime.timezone.utc)
    return max(0.0, (when - now).total_seconds())


def retry_delay(attempt, retry_after=None, rng=random.random):
    """Seconds to wait before retry number `attempt` (1-based): Retry-After wins, else backoff with jitter."""
    if retry_after is not None:
        return min(retry_after, RETRY_AFTER_CAP)
    ceiling = min(BACKOFF_CAP, BACKOFF_BASE * 2 ** (attempt - 1))
    return ceiling / 2 + rng() * ceiling / 2


def error_details(body):
    """(message, code) from an API error body."""
    try:
        error = json.loads(body).get('error') or {}
        return str(error.get('message', ''))[:ERROR_TEXT_LIMIT], error.get('code')
    except (ValueError, AttributeError):
        return body[:ERROR_TEXT_LIMIT].decode('utf-8', 'replace') if isinstance(body, bytes) else '', None


# Root certificate bundles to fall back to when this Python has none of its own (the python.org
# installer on macOS ships without them until "Install Certificates.command" runs). Verification
# stays on; only where the trusted roots come from changes.
CA_BUNDLE_ENV = 'SSL_CERT_FILE'
FALLBACK_CA_BUNDLES = ('/etc/ssl/cert.pem', '/opt/homebrew/etc/openssl@3/cert.pem',
                       '/usr/local/etc/openssl@3/cert.pem', '/etc/ssl/certs/ca-certificates.crt')


def find_ca_bundle(paths=None, exists=os.path.isfile, env=os.environ):
    """The CA file to trust, or None when Python's own default already has one (or none exists)."""
    if env.get(CA_BUNDLE_ENV):
        return None  # OpenSSL reads SSL_CERT_FILE itself
    if paths is None:
        paths = ssl.get_default_verify_paths()
    if (paths.cafile and exists(paths.cafile)) or (paths.openssl_cafile and exists(paths.openssl_cafile)):
        return None
    try:
        import certifi  # optional
        return certifi.where()
    except ImportError:
        pass
    return next((path for path in FALLBACK_CA_BUNDLES if exists(path)), None)


def tls_context():
    """A verifying TLS context whose trusted roots also work on a certificate-less Python."""
    return ssl.create_default_context(cafile=find_ca_bundle())


def default_opener(request, timeout):
    return urllib.request.urlopen(request, timeout=timeout, context=tls_context())


def is_certificate_error(error):
    reason = getattr(error, 'reason', error)
    return isinstance(reason, ssl.SSLCertVerificationError)


class ImagesClient:
    """Posts edit requests with retries. `opener` and `sleep` are injectable for tests."""

    def __init__(self, api_key, api_base=API_BASE, timeout=DEFAULT_TIMEOUT, attempts=DEFAULT_ATTEMPTS,
                 log=None, opener=default_opener, sleep=time.sleep, rng=random.random):
        self._key = api_key
        self._base = check_api_base(api_base)
        self._timeout = timeout
        self._attempts = attempts
        self._log = log or (lambda record: None)
        self._open = opener
        self._sleep = sleep
        self._rng = rng

    def edit(self, fields, images, tag=None):
        """images [(filename, bytes, content type)] -> (response json, request id, attempts)."""
        if not 1 <= len(images) <= MAX_IMAGES:
            raise ApiError(f'an edit takes 1..{MAX_IMAGES} images, not {len(images)}')
        files = [(IMAGE_FIELD, name, data, content_type) for name, data, content_type in images]
        body, content_type = encode_multipart(fields, files)
        return self._post(EDITS_PATH, body, content_type, tag)

    def _request(self, path, body, content_type):
        headers = {AUTH_HEADER: f'Bearer {self._key}', 'Content-Type': content_type}
        self._log({'event': 'request', 'url': self._base + path, 'headers': redact_headers(headers),
                   'bytes': len(body)})
        return urllib.request.Request(self._base + path, data=body, headers=headers, method='POST')

    def _post(self, path, body, content_type, tag):
        for attempt in range(1, self._attempts + 1):
            try:
                with self._open(self._request(path, body, content_type), timeout=self._timeout) as response:
                    request_id = response.headers.get(REQUEST_ID_HEADER)
                    payload = json.loads(response.read())
                self._log({'event': 'response', 'tag': tag, 'attempt': attempt, 'request_id': request_id})
                return payload, request_id, attempt
            except urllib.error.HTTPError as error:
                delay = self._http_failure(error, attempt, tag)
            except (urllib.error.URLError, socket.timeout, TimeoutError, ConnectionError) as error:
                delay = self._network_failure(error, attempt, tag)
            self._sleep(delay)
        raise AssertionError('unreachable')

    def _http_failure(self, error, attempt, tag):
        request_id = error.headers.get(REQUEST_ID_HEADER) if error.headers else None
        message, code = error_details(error.read() or b'')
        message = scrub(message, self._key)
        retryable = error.code in RETRY_STATUSES and code not in NO_RETRY_CODES
        self._log({'event': 'http-error', 'tag': tag, 'attempt': attempt, 'status': error.code,
                   'code': code, 'message': message, 'request_id': request_id})
        if not retryable or attempt == self._attempts:
            raise ApiError(f'HTTP {error.code}: {message}', error.code, request_id)
        retry_after = parse_retry_after(error.headers.get('Retry-After') if error.headers else None)
        return self._delay(attempt, retry_after, tag)

    def _network_failure(self, error, attempt, tag):
        message = scrub(error, self._key)
        self._log({'event': 'network-error', 'tag': tag, 'attempt': attempt, 'message': message})
        if is_certificate_error(error):
            # Not transient: retrying cannot help. Nothing was sent (TLS failed before the request).
            raise ApiError(f'TLS certificate check failed: {message}. Set {CA_BUNDLE_ENV} to a CA bundle '
                           'or run Python\'s "Install Certificates.command".')
        if attempt == self._attempts:
            raise ApiError(f'network error: {message}')
        return self._delay(attempt, None, tag)

    def _delay(self, attempt, retry_after, tag):
        delay = retry_delay(attempt, retry_after, self._rng)
        self._log({'event': 'retry', 'tag': tag, 'attempt': attempt, 'delay_s': round(delay, 2),
                   'retry_after': retry_after})
        return delay
