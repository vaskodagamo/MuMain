"""Just enough C++ to read the client's model-loading code; standard library only.

The item model table (item_models.json) is generated from the engine's own load functions
(OpenPlayers, OpenItems, ...) instead of being typed by hand. Those functions are plain
straight-line C++: loops over int counters, ifs, a few local string buffers filled with
mu_swprintf, and calls such as gLoadData.AccessModel(MODEL_SWORD + i, L"Data\\Item\\", L"Sword", i + 1).
This module provides the three pieces needed to run them symbolically:

- source text handling: comments removed, #if/#ifdef regions resolved, functions located;
- a symbol table of the integer constants in the global headers (enums, #define, constexpr);
- a small interpreter that walks a function body and reports every recognised call with its
  evaluated arguments.

Anything the interpreter does not understand is skipped; a skipped statement that mentions a call
it is looking for is reported, so nothing disappears silently.
"""

from pathlib import Path
import re

IDENT = re.compile(r'[A-Za-z_]\w*')
NUMBER = re.compile(r'(0[xX][0-9a-fA-F]+|\d+)([uUlL]*)')
PUNCTUATORS = ('<<=', '>>=', '::', '->', '++', '--', '<<', '>>', '<=', '>=', '==', '!=', '&&', '||',
               '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=')
SINGLE_PUNCTUATORS = '{}()[];,.<>+-*/%=!&|^~?:#'
TYPE_WORDS = {'int', 'unsigned', 'short', 'long', 'char', 'wchar_t', 'const', 'auto', 'float', 'bool',
              'BYTE', 'WORD', 'DWORD', 'size_t', 'static', 'constexpr'}
CAST_TYPES = {'int', 'unsigned', 'short', 'long', 'BYTE', 'WORD', 'DWORD'}
ESCAPES = {'\\': '\\', '"': '"', "'": "'", 'n': '\n', 't': '\t', '0': '\0'}
DIRECTIVE = re.compile(r'^\s*#\s*(\w+)(.*)$')
DEFINE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)(?!\()(.*)$')
DEFINE_NAME = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)')


class Unresolved(Exception):
    """An expression uses something the symbol table or the interpreter does not know."""


# --- source text -----------------------------------------------------------------------------

def strip_comments(text):
    """Blank // and /* */ comments outside string and character literals; newlines are kept."""
    out, i, length = [], 0, len(text)
    while i < length:
        char = text[i]
        pair = text[i:i + 2]
        if pair == '//':
            end = text.find('\n', i)
            end = length if end < 0 else end
            out.append(' ' * (end - i))
            i = end
        elif pair == '/*':
            end = text.find('*/', i + 2)
            end = length if end < 0 else end + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:end]))
            i = end
        elif char in '"\'':
            end = i + 1
            while end < length and text[end] != char:
                end += 2 if text[end] == '\\' else 1
            out.append(text[i:end + 1])
            i = end + 1
        else:
            out.append(char)
            i += 1
    return ''.join(out)


def defined_macros(header_texts):
    """Names #define-d anywhere in the given (comment-free) header texts."""
    names = set()
    for text in header_texts:
        for line in text.splitlines():
            match = DEFINE_NAME.match(line)
            if match:
                names.add(match.group(1))
    return names


def condition_value(expression, macros, consulted):
    """Value of an #if expression made of defined(X), X, 0/1, !, && and ||."""
    def replace_defined(match):
        name = match.group(1) or match.group(2)
        consulted[name] = name in macros
        return ' 1 ' if name in macros else ' 0 '
    text = re.sub(r'defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)', replace_defined, expression)
    text = text.replace('&&', ' and ').replace('||', ' or ')
    text = re.sub(r'!(?!=)', ' not ', text)

    def replace_name(match):
        word = match.group(0)
        if word in ('and', 'or', 'not'):
            return word
        consulted[word] = word in macros
        return '1' if word in macros else '0'
    text = re.sub(r'[A-Za-z_]\w*', replace_name, text)
    if not re.fullmatch(r'[\s01()andornt]*', text):
        raise Unresolved(f'#if expression {expression.strip()!r}')
    return bool(eval(text, {'__builtins__': {}}))  # only 0/1, parentheses and boolean words remain


def resolve_conditionals(text, macros, consulted):
    """Blank the lines of inactive #if regions and every directive but #define; line numbers are kept."""
    lines = text.split('\n')
    stack = []  # (region active, some branch taken)
    active = True
    for number, line in enumerate(lines):
        match = DIRECTIVE.match(line)
        if not match:
            if not active:
                lines[number] = ''
            continue
        word, rest = match.group(1), match.group(2)
        if word in ('if', 'ifdef', 'ifndef'):
            if word == 'if':
                value = condition_value(rest, macros, consulted) if active else False
            else:
                name = rest.split()[0]
                consulted[name] = name in macros
                value = (name in macros) == (word == 'ifdef')
            stack.append((active, value))
            active = active and value
        elif word == 'elif' and stack:
            parent, taken = stack[-1]
            value = parent and not taken and condition_value(rest, macros, consulted)
            stack[-1] = (parent, taken or value)
            active = value
        elif word == 'else' and stack:
            parent, taken = stack[-1]
            stack[-1] = (parent, True)
            active = parent and not taken
        elif word == 'endif' and stack:
            active = stack.pop()[0]
        elif word == 'define' and active:
            continue
        lines[number] = ''
    return '\n'.join(lines)


def function_body(text, qualified_name):
    """The text between the braces of `<type> qualified_name(...)` and its line number."""
    pattern = re.compile(r'\b\w[\w\s\*&:<>]*?\b' + re.escape(qualified_name) + r'\s*\([^;{)]*\)\s*(const\s*)?\{')
    match = pattern.search(text)
    if not match:
        return None, None
    start = match.end()
    depth, i = 1, start
    while depth and i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
        elif text[i] in '"\'':
            quote, i = text[i], i + 1
            while text[i] != quote:
                i += 2 if text[i] == '\\' else 1
        i += 1
    return text[start:i - 1], text.count('\n', 0, start) + 1


# --- tokens ---------------------------------------------------------------------------------

class Token:
    __slots__ = ('kind', 'value', 'line')

    def __init__(self, kind, value, line):
        self.kind, self.value, self.line = kind, value, line

    def __repr__(self):
        return f'{self.kind}:{self.value!r}'


def unescape(body):
    out, i = [], 0
    while i < len(body):
        if body[i] == '\\' and i + 1 < len(body):
            out.append(ESCAPES.get(body[i + 1], body[i + 1]))
            i += 2
        else:
            out.append(body[i])
            i += 1
    return ''.join(out)


def tokenize(text, first_line=1):
    tokens, i, line = [], 0, first_line
    while i < len(text):
        char = text[i]
        if char == '\n':
            line += 1
            i += 1
        elif char.isspace():
            i += 1
        elif char in 'Lu' and text[i + 1:i + 2] in ('"', "'") or char in '"\'':
            quote_at = i + 1 if char in 'Lu' else i
            quote = text[quote_at]
            end = quote_at + 1
            while text[end] != quote:
                end += 2 if text[end] == '\\' else 1
            value = unescape(text[quote_at + 1:end])
            tokens.append(Token('str' if quote == '"' else 'char', value, line))
            i = end + 1
        elif char.isdigit():
            match = NUMBER.match(text, i)
            tokens.append(Token('num', int(match.group(1), 0), line))
            i = match.end()
            if text[i:i + 1] in ('.', 'f', 'F'):
                float_match = re.compile(r'[\d.]*[fF]?').match(text, i)
                tokens[-1] = Token('float', text[match.start():float_match.end()], line)
                i = float_match.end()
        elif char.isalpha() or char == '_':
            match = IDENT.match(text, i)
            tokens.append(Token('id', match.group(0), line))
            i = match.end()
        else:
            for punctuator in PUNCTUATORS:
                if text.startswith(punctuator, i):
                    tokens.append(Token('op', punctuator, line))
                    i += len(punctuator)
                    break
            else:
                if char not in SINGLE_PUNCTUATORS:
                    raise Unresolved(f'unexpected character {char!r} on line {line}')
                tokens.append(Token('op', char, line))
                i += 1
    return tokens


# --- expressions ----------------------------------------------------------------------------

BINARY = {
    '||': 1, '&&': 2, '|': 3, '^': 4, '&': 5, '==': 6, '!=': 6, '<': 7, '>': 7, '<=': 7, '>=': 7,
    '<<': 8, '>>': 8, '+': 9, '-': 9, '*': 10, '/': 10, '%': 10,
}


def c_divide(left, right):
    quotient = abs(left) // abs(right)
    return quotient if (left >= 0) == (right >= 0) else -quotient


def apply_binary(op, left, right):
    if op in ('+', '-', '*', '/', '%') and (isinstance(left, str) or isinstance(right, str)):
        raise Unresolved('arithmetic on a string')
    table = {
        '||': lambda: int(bool(left) or bool(right)), '&&': lambda: int(bool(left) and bool(right)),
        '|': lambda: left | right, '^': lambda: left ^ right, '&': lambda: left & right,
        '==': lambda: int(left == right), '!=': lambda: int(left != right),
        '<': lambda: int(left < right), '>': lambda: int(left > right),
        '<=': lambda: int(left <= right), '>=': lambda: int(left >= right),
        '<<': lambda: left << right, '>>': lambda: left >> right,
        '+': lambda: left + right, '-': lambda: left - right, '*': lambda: left * right,
        '/': lambda: c_divide(left, right), '%': lambda: left - c_divide(left, right) * right,
    }
    return table[op]()


class ExpressionParser:
    """Precedence-climbing evaluator over a token list; lookup(name) resolves identifiers."""

    def __init__(self, tokens, lookup):
        self.tokens, self.lookup, self.pos = tokens, lookup, 0

    def peek(self, offset=0):
        index = self.pos + offset
        return self.tokens[index] if index < len(self.tokens) else None

    def is_op(self, value, offset=0):
        token = self.peek(offset)
        return token is not None and token.kind == 'op' and token.value == value

    def take(self, value=None):
        token = self.peek()
        if token is None or (value is not None and not (token.kind == 'op' and token.value == value)):
            raise Unresolved(f'expected {value!r}, found {token!r}')
        self.pos += 1
        return token

    def parse(self):
        value = self.ternary()
        if self.pos != len(self.tokens):
            raise Unresolved(f'unexpected {self.peek()!r} in expression')
        return value

    def ternary(self):
        condition = self.binary(1)
        if not self.is_op('?'):
            return condition
        self.take('?')
        when_true = self.ternary()
        self.take(':')
        when_false = self.ternary()
        return when_true if condition else when_false

    def binary(self, minimum):
        left = self.unary()
        while True:
            token = self.peek()
            if token is None or token.kind != 'op' or token.value not in BINARY or BINARY[token.value] < minimum:
                return left
            self.pos += 1
            right = self.binary(BINARY[token.value] + 1)
            left = apply_binary(token.value, left, right)

    def unary(self):
        token = self.peek()
        if token is not None and token.kind == 'op' and token.value in ('-', '+', '!', '~'):
            self.pos += 1
            value = self.unary()
            return {'-': lambda: -value, '+': lambda: value, '!': lambda: int(not value), '~': lambda: ~value}[token.value]()
        if self.is_op('(') and self.is_cast():
            self.skip_cast()
            return self.unary()
        return self.postfix(self.primary())

    def is_cast(self):
        inner = self.peek(1)
        return inner is not None and inner.kind == 'id' and inner.value in CAST_TYPES and self.is_op(')', 2)

    def skip_cast(self):
        self.pos += 3

    def primary(self):
        token = self.take()
        if token.kind in ('num', 'str', 'char'):
            return token.value if token.kind != 'char' else ord(token.value)
        if token.kind == 'op' and token.value == '(':
            value = self.ternary()
            self.take(')')
            return value
        if token.kind == 'op' and token.value == '::':
            return self.primary()
        if token.kind == 'id' and token.value in ('static_cast', 'reinterpret_cast'):
            self.take('<')
            while not self.is_op('>'):
                self.pos += 1
            self.take('>')
            self.take('(')
            value = self.ternary()
            self.take(')')
            return value
        if token.kind == 'id':
            return self.lookup(token.value)
        raise Unresolved(f'cannot evaluate {token!r}')

    def postfix(self, value):
        while self.is_op('['):
            self.take('[')
            index = self.ternary()
            self.take(']')
            if not isinstance(value, list):
                raise Unresolved('indexing a non-array')
            value = value[index]
        return value


def evaluate(tokens, lookup):
    if not tokens:
        raise Unresolved('empty expression')
    return ExpressionParser(tokens, lookup).parse()


# --- symbol table ---------------------------------------------------------------------------

ENUM_START = re.compile(r'\benum\b(?:\s+(?:class|struct))?(?:\s+[A-Za-z_]\w*)?(?:\s*:\s*[\w:\s]+?)?\s*\{')
CONSTEXPR = re.compile(r'\b(?:static\s+)?constexpr\s+[\w:<>\s]+?\b([A-Za-z_]\w*)\s*=\s*([^;{}]+);')


def split_top_level(tokens, separator=','):
    parts, current, depth = [], [], 0
    for token in tokens:
        if token.kind == 'op' and token.value in '([{':
            depth += 1
        elif token.kind == 'op' and token.value in ')]}':
            depth -= 1
        if depth == 0 and token.kind == 'op' and token.value == separator:
            parts.append(current)
            current = []
        else:
            current.append(token)
    if current:
        parts.append(current)
    return parts


class SymbolTable:
    """Integer constants from enums, #define and constexpr, resolved on first use."""

    def __init__(self):
        self.definitions = {}  # name -> ('expr', tokens) | ('next', previous name) | ('value', int)
        self.values = {}
        self.resolving = set()

    def add_header(self, text):
        """Collect the constants of one comment-free, conditional-resolved header text."""
        for line in text.split('\n'):
            match = DEFINE.match(line)
            if match and match.group(2).strip():
                self.add_expression(match.group(1), match.group(2))
        for match in CONSTEXPR.finditer(text):
            self.add_expression(match.group(1), match.group(2))
        for match in ENUM_START.finditer(text):
            end = text.find('}', match.end())
            self.add_enum(tokenize(text[match.end():end]))

    def add_expression(self, name, text):
        """A #define or constexpr value; macros that are not expressions (multi-line, strings) are left out."""
        try:
            self.add(name, ('expr', tokenize(text)))
        except Unresolved:
            pass

    def add_enum(self, tokens):
        previous = None
        for entry in split_top_level(tokens):
            if not entry or entry[0].kind != 'id':
                continue
            name = entry[0].value
            if len(entry) > 2 and entry[1].kind == 'op' and entry[1].value == '=':
                self.add(name, ('expr', entry[2:]))
            elif previous is None:
                self.add(name, ('value', 0))
            else:
                self.add(name, ('next', previous))
            previous = name

    def add(self, name, definition):
        self.definitions.setdefault(name, definition)

    def value(self, name):
        if name in self.values:
            return self.values[name]
        definition = self.definitions.get(name)
        if definition is None or name in self.resolving:
            raise Unresolved(f'unknown constant {name}')
        self.resolving.add(name)
        try:
            kind, payload = definition
            if kind == 'value':
                result = payload
            elif kind == 'next':
                result = self.value(payload) + 1
            else:
                result = evaluate(payload, self.value)
        finally:
            self.resolving.discard(name)
        if not isinstance(result, int):
            raise Unresolved(f'{name} is not an integer constant')
        self.values[name] = result
        return result


def read_source(path, macros, consulted):
    text = strip_comments(Path(path).read_text(encoding='utf-8', errors='replace'))
    return resolve_conditionals(text, macros, consulted)
