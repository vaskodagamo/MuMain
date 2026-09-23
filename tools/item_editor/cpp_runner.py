"""Walk a C++ function body and collect the calls the item tools care about.

Supported: blocks, `for (init; cond; step)`, `if/else`, `continue`, `break`, local declarations of
ints and wide-string buffers or arrays, `x++`, `x += n`, `x = expr`, `mu_swprintf(buffer, fmt, ...)`
(%ls, %s, %hs, %d with width), calls to the watched functions, and argument-less member calls
(`g_CMonkSystem.LoadModelItem()`) whose definition is found and itself mentions a watched call.
Everything else (LoadBitmap, Models[...] = ..., new ...) is skipped; a skipped statement that
mentions a watched call is recorded in `skipped`.
"""

import re

from cpp_source import Unresolved, evaluate, split_top_level, tokenize, TYPE_WORDS

MAX_LOOP_ITERATIONS = 100000
MAX_CALL_DEPTH = 4
FORMAT_SPEC = re.compile(r'%(0?\d*)(ls|hs|s|d|i)')


class LoopSignal(Exception):
    pass


class ContinueSignal(LoopSignal):
    pass


class BreakSignal(LoopSignal):
    pass


class ReturnSignal(Exception):
    pass


def c_format(fmt, values):
    """mu_swprintf with the conversions the load code uses."""
    remaining = list(values)

    def substitute(match):
        if not remaining:
            raise Unresolved(f'too few arguments for {fmt!r}')
        value = remaining.pop(0)
        width, conversion = match.group(1), match.group(2)
        if conversion in ('d', 'i'):
            if not isinstance(value, int):
                raise Unresolved('%d needs an integer')
            return f'{value:{width}d}' if width else str(value)
        if not isinstance(value, str):
            raise Unresolved('%s needs a string')
        return value
    return FORMAT_SPEC.sub(substitute, fmt)


class Call:
    """One watched call: function name, evaluated arguments, the function it ran in."""

    def __init__(self, name, args, function, line):
        self.name, self.args, self.function, self.line = name, args, function, line


class Runner:
    def __init__(self, symbols, watched, resolve_method=None):
        """watched: names of calls to record; resolve_method(name) -> (qualified name, body, first line) or None."""
        self.symbols = symbols
        self.watched = set(watched)
        self.resolve_method = resolve_method
        self.calls = []
        self.skipped = []
        self.depth = 0

    # --- driver ---------------------------------------------------------------------------

    def run(self, function, body, first_line):
        tokens = tokenize(body, first_line)
        frame = _Frame(self, function, tokens)
        while frame.pos < len(tokens):
            try:
                frame.statement(True)
            except ReturnSignal:
                break
        return self.calls


class _Frame:
    """Execution state of one function body."""

    def __init__(self, runner, function, tokens):
        self.runner, self.function, self.tokens = runner, function, tokens
        self.pos = 0
        self.locals = {}

    # --- token helpers --------------------------------------------------------------------

    def peek(self, offset=0):
        index = self.pos + offset
        return self.tokens[index] if index < len(self.tokens) else None

    def at(self, value, offset=0):
        token = self.peek(offset)
        return token is not None and token.value == value and token.kind in ('op', 'id')

    def take(self, value=None):
        token = self.peek()
        if token is None or (value is not None and token.value != value):
            raise Unresolved(f'{self.function}: expected {value!r}, found {token!r}')
        self.pos += 1
        return token

    def balanced(self, open_char, close_char):
        """Tokens inside the bracket pair starting at pos; pos ends after the closing bracket."""
        self.take(open_char)
        start, depth = self.pos, 1
        while depth:
            token = self.take()
            if token.kind == 'op' and token.value == open_char:
                depth += 1
            elif token.kind == 'op' and token.value == close_char:
                depth -= 1
        return self.tokens[start:self.pos - 1]

    def until_semicolon(self):
        start, depth = self.pos, 0
        while True:
            token = self.take()
            if token.kind == 'op' and token.value in '([{':
                depth += 1
            elif token.kind == 'op' and token.value in ')]}':
                depth -= 1
            elif token.kind == 'op' and token.value == ';' and depth == 0:
                return self.tokens[start:self.pos - 1]

    def lookup(self, name):
        if name in self.locals:
            value = self.locals[name]
            if value is None:
                raise Unresolved(f'{name} has no known value')
            return value
        return self.runner.symbols.value(name)

    def value_of(self, tokens):
        return evaluate(tokens, self.lookup)

    # --- statements -----------------------------------------------------------------------

    def statement(self, active):
        token = self.peek()
        if token.kind == 'op' and token.value == '{':
            self.block(active)
        elif token.kind == 'op' and token.value == ';':
            self.take()
        elif token.kind == 'id' and token.value == 'for':
            self.for_loop(active)
        elif token.kind == 'id' and token.value == 'if':
            self.if_else(active)
        elif token.kind == 'id' and token.value in ('continue', 'break', 'return'):
            self.take()
            self.until_semicolon()
            if active:
                raise {'continue': ContinueSignal, 'break': BreakSignal, 'return': ReturnSignal}[token.value]()
        else:
            tokens = self.until_semicolon()
            if active:
                self.simple(tokens)

    def block(self, active):
        self.take('{')
        while not self.at('}'):
            self.statement(active)
        self.take('}')

    def if_else(self, active):
        self.take('if')
        condition = self.balanced('(', ')')
        body_start = self.pos
        try:
            value = bool(self.value_of(condition)) if active else False
        except Unresolved as error:
            self.skip_unknown_branch(body_start, str(error))
            active = value = False
        self.statement(active and value)
        if self.at('else'):
            self.take('else')
            self.statement(active and not value)

    def skip_unknown_branch(self, body_start, reason):
        """A condition that cannot be evaluated: skip its statements, report watched calls in them."""
        self.statement(False)
        if self.at('else'):
            self.take('else')
            self.statement(False)
        self.note_skipped(self.tokens[body_start:self.pos], reason)
        self.pos = body_start

    def for_loop(self, active):
        self.take('for')
        init, condition, step = (split_top_level(self.balanced('(', ')'), ';') + [[], [], []])[:3]
        body_start = self.pos
        if active:
            try:
                self.iterate(init, condition, step, body_start)
            except Unresolved as error:
                self.pos = body_start
                self.statement(False)
                self.note_skipped(self.tokens[body_start:self.pos], str(error))
        self.pos = body_start
        self.statement(False)

    def iterate(self, init, condition, step, body_start):
        self.simple(init)
        for _ in range(MAX_LOOP_ITERATIONS):
            if condition and not self.value_of(condition):
                return
            self.pos = body_start
            try:
                self.statement(True)
            except ContinueSignal:
                pass
            except BreakSignal:
                return
            self.simple(step)
        raise Unresolved(f'{self.function}: loop did not end')

    # --- simple statements ----------------------------------------------------------------

    def simple(self, tokens):
        if not tokens:
            return
        while tokens and tokens[0].value == '::':
            tokens = tokens[1:]
        try:
            self.dispatch(tokens)
        except Unresolved as error:
            self.note_skipped(tokens, str(error))

    def dispatch(self, tokens):
        first = tokens[0]
        if first.kind == 'id' and first.value in TYPE_WORDS:
            self.declare(tokens)
        elif first.value in ('++', '--') and len(tokens) == 2:
            self.increment(tokens[1].value, 1 if first.value == '++' else -1)
        elif len(tokens) == 2 and tokens[1].value in ('++', '--'):
            self.increment(first.value, 1 if tokens[1].value == '++' else -1)
        elif len(tokens) > 2 and first.kind == 'id' and first.value in self.locals and tokens[1].value in ('=', '+=', '-='):
            self.assign(first.value, tokens[1].value, tokens[2:])
        else:
            self.call(tokens)

    def declare(self, tokens):
        index = 0
        while index < len(tokens) and (tokens[index].value in TYPE_WORDS or tokens[index].value in ('*', '&')):
            index += 1
        if index >= len(tokens) or tokens[index].kind != 'id':
            raise Unresolved('declaration without a name')
        name, index = tokens[index].value, index + 1
        dimensions = 0
        while index < len(tokens) and tokens[index].value == '[':
            while tokens[index].value != ']':
                index += 1
            index, dimensions = index + 1, dimensions + 1
        self.locals[name] = None
        if index < len(tokens) and tokens[index].value == '=':
            self.locals[name] = self.initial_value(tokens[index + 1:], dimensions)
        elif dimensions:
            self.locals[name] = ''

    def initial_value(self, tokens, dimensions):
        if not (tokens and tokens[0].value == '{'):
            return self.value_of(tokens)
        elements = [self.value_of(part) for part in split_top_level(tokens[1:-1])]
        if dimensions >= 2 or len(elements) != 1:
            return elements
        return elements[0]

    def increment(self, name, step):
        self.locals[name] = self.lookup(name) + step

    def assign(self, name, operator, value_tokens):
        value = self.value_of(value_tokens)
        if operator == '=':
            self.locals[name] = value
        else:
            self.locals[name] = self.lookup(name) + (value if operator == '+=' else -value)

    def call(self, tokens):
        open_at = next((i for i, t in enumerate(tokens) if t.kind == 'op' and t.value == '('), None)
        if open_at is None or open_at == 0 or tokens[open_at - 1].kind != 'id' or tokens[-1].value != ')':
            raise Unresolved('not a call')
        name = tokens[open_at - 1].value
        arguments = split_top_level(tokens[open_at + 1:-1])
        if name in self.runner.watched:
            values = [self.value_of(argument) for argument in arguments]
            self.runner.calls.append(Call(name, values, self.function, tokens[0].line))
        elif name in ('mu_swprintf', 'swprintf', 'wsprintf'):
            self.format_into(arguments)
        elif not arguments and open_at >= 2 and tokens[open_at - 2].value in ('.', '->'):
            self.follow(name)
        elif self.mentions_watched(tokens):
            raise Unresolved(f'call {name} is not understood')

    def format_into(self, arguments):
        if len(arguments) < 2 or len(arguments[0]) != 1:
            raise Unresolved('mu_swprintf into something other than a local buffer')
        target = arguments[0][0].value
        fmt = self.value_of(arguments[1])
        self.locals[target] = c_format(fmt, [self.value_of(argument) for argument in arguments[2:]])

    def follow(self, method):
        runner = self.runner
        found = runner.resolve_method(method) if runner.resolve_method else None
        if found is None:
            return
        qualified, body, first_line = found
        if not any(word in body for word in runner.watched) or runner.depth >= MAX_CALL_DEPTH:
            return
        runner.depth += 1
        try:
            frame = _Frame(runner, qualified, tokenize(body, first_line))
            while frame.pos < len(frame.tokens):
                try:
                    frame.statement(True)
                except ReturnSignal:
                    break
        finally:
            runner.depth -= 1

    def mentions_watched(self, tokens):
        return any(token.kind == 'id' and token.value in self.runner.watched for token in tokens)

    def note_skipped(self, tokens, reason):
        if self.mentions_watched(tokens):
            text = ' '.join(str(token.value) for token in tokens)
            self.runner.skipped.append({'function': self.function, 'statement': text, 'reason': reason})
