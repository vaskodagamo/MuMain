"""Read the client's item render code: branches, their item types and their calls.

What is still code is of three kinds:
- if / else-if chains and switches keyed on the model type (ItemObjectAttribute,
  RenderPartObjectBody and RenderModelWithoutStyle in Engine/Object/ZzzObject.cpp;
  SpawnLinkedPartEffects and SpawnHeldWeaponGlow in Engine/Object/ZzzCharacter.cpp);
- a row of ifs that each return (ApplyEventModelEffect in ZzzObject.cpp);
- one function per named look, listed in a table of names (the render styles of
  Render/Items/ItemRenderStyles.cpp and the item effects of Render/Items/ItemEffects.cpp).
This module finds those branches and functions in the preprocessed source (cpp_source.read_source:
comments blanked, inactive #if regions removed, line numbers kept), works out which item types a
branch's condition selects, lists the calls and assignments inside it with their line numbers, and
says whether a statement runs given a condition decider (guard). It does not execute anything;
what a call means is decided by render_rules.py and render_facts.py.
"""

from dataclasses import dataclass, field
import re

from cpp_source import Unresolved, evaluate, function_body, tokenize

TYPE_WORD = r'(?<![\w>])(?:o->Type|Type|w->Type|g_CMonkSystem\.EqualItemModelType\(Type\))'
OPERAND = r'(?:static_cast<int>\()?[A-Z][A-Z0-9_]*\)?(?:\s*[+-]\s*(?:\d+|[A-Z][A-Z0-9_]*))?'
EQUALS = [re.compile(rf'{TYPE_WORD}\s*==\s*({OPERAND})'), re.compile(rf'({OPERAND})\s*==\s*{TYPE_WORD}')]
RANGES = [
    (re.compile(rf'{TYPE_WORD}\s*>=\s*({OPERAND})\s*&&\s*{TYPE_WORD}\s*(<=|<)\s*({OPERAND})'), 1, 3, 2),
    (re.compile(rf'({OPERAND})\s*<=\s*{TYPE_WORD}\s*&&\s*({OPERAND})\s*(>=|>)\s*{TYPE_WORD}'), 1, 2, 3),
]
# Condition parts that do not change what a normal item draw does.
IGNORED_QUALIFIERS = ('!(RenderType & RENDER_DOPPELGANGER)', 'b->NumMeshs')

CALL_NAMES = ('RenderMesh', 'RenderBody', 'RenderPartObjectBodyColor2', 'RenderPartObjectBodyColor',
              'CreateSprite', 'CreateParticle', 'CreateParticleFpsChecked', 'CreateJoint', 'CreateEffect',
              'RenderBrightEffect', 'SpawnBrightSprite', 'PlayBuffer', 'RenderLight', 'CreateSpriteFpsChecked',
              'RenderSetChrome')
CALL = re.compile(r'\b(' + '|'.join(CALL_NAMES) + r')\s*\(')
# The fields of the object, of the model, and of the draw's own copy of them (DrawLook) under the
# object's names.
LOOK_FIELDS = {'alpha': 'Alpha', 'blendMesh': 'BlendMesh', 'blendMeshLight': 'BlendMeshLight',
               'blendMeshU': 'BlendMeshTexCoordU', 'blendMeshV': 'BlendMeshTexCoordV', 'hiddenMesh': 'HiddenMesh'}
ASSIGN = re.compile(r'(?:(?:\bo->|\bObject->|\bb->|Models\[[^\]]+\]\.)'
                    r'(BlendMesh|HiddenMesh|BlendMeshLight|BlendMeshTexCoordU|BlendMeshTexCoordV|StreamMesh|Alpha)'
                    r'|\blook\.(' + '|'.join(LOOK_FIELDS) + r'))'
                    r'\s*=(?!=)\s*([^;]+);')
LEVEL_ASSIGN = re.compile(r'\bLevel\s*(=(?!=)|\+=|-=|--|\+\+)\s*([^;]*);?')
CONTROL = re.compile(r'\b(if|else|for|while|switch|case|default)\b')
RETURN = re.compile(r'\breturn\s*;')
RETURN_VALUE = re.compile(r'\breturn\s+([^;]+);')


class CodeError(RuntimeError):
    """The source no longer has the structure a region expects (an anchor moved or vanished)."""


@dataclass(frozen=True)
class Anchor:
    """A line of an engine file (path below src/source), found as the last of `needles`, each
    searched after the previous one; `what` says what the line shows."""
    file: str
    needles: tuple
    what: str = ''


@dataclass
class Call:
    name: str
    args: list
    line: int
    conditional: bool  # inside a nested if/for/switch of the branch (may not run every frame)
    text: str
    position: int = 0  # offset in the branch body


@dataclass
class Assignment:
    target: str
    value: str
    line: int
    conditional: bool
    position: int = 0


@dataclass
class Branch:
    """One arm of an if-chain or one case group of a switch."""
    function: str
    line: int                  # line of the condition / first case label
    condition: str             # C++ text, '' for else / default
    types: list = field(default_factory=list)   # model type values the condition selects
    qualifiers: list = field(default_factory=list)  # other parts of the condition
    unresolved: list = field(default_factory=list)  # operands the symbol table does not know
    calls: list = field(default_factory=list)
    assignments: list = field(default_factory=list)
    level: list = field(default_factory=list)   # (operator, value text, line, offset in the body)
    returns: bool = False
    body_line: int = 0
    body: str = ''
    file: str = ''          # set by the caller: the engine file of the region
    contexts: tuple = ()    # set by the caller: worn / dropped / inventory


def line_of(text, position, first_line):
    return first_line + text.count('\n', 0, position)


def skip_space(text, position):
    while position < len(text) and text[position].isspace():
        position += 1
    return position


def matching(text, position, opening, closing):
    """Index just past the bracket that closes the one at `position`; string literals are skipped."""
    depth = 0
    index = position
    while index < len(text):
        char = text[index]
        if char in '"\'':
            end = index + 1
            while end < len(text) and text[end] != char:
                end += 2 if text[end] == '\\' else 1
            index = end + 1
            continue
        if char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    raise CodeError(f'unbalanced {opening}{closing}')


def statement_end(text, position):
    """End of one statement at `position`: a {} block, or up to the ';' at depth 0."""
    position = skip_space(text, position)
    if position < len(text) and text[position] == '{':
        return matching(text, position, '{', '}')
    depth = 0
    index = position
    while index < len(text):
        char = text[index]
        if char in '({[':
            depth += 1
        elif char in ')}]':
            depth -= 1
        elif char == ';' and depth == 0:
            return index + 1
        index += 1
    raise CodeError('statement without ;')


def keyword_at(text, position, word):
    return re.match(rf'{word}\b', text[position:]) is not None


def parse_if_chain(text, position, first_line, spans=None):
    """The arms of the if / else if / else chain starting at `position` (which must be at 'if').
    Returns ([(condition, condition_line, body, body_line)], end position); `spans` (optional) gets
    (condition, body start, body end) per arm."""
    arms = []
    spans = [] if spans is None else spans
    while True:
        position = skip_space(text, position)
        if not keyword_at(text, position, 'if'):
            raise CodeError(f'expected if at line {line_of(text, position, first_line)}')
        open_paren = text.index('(', position)
        close_paren = matching(text, open_paren, '(', ')')
        condition = text[open_paren + 1:close_paren - 1]
        body_start = skip_space(text, close_paren)
        body_end = statement_end(text, body_start)
        arms.append((condition, line_of(text, position, first_line), text[body_start:body_end],
                     line_of(text, body_start, first_line)))
        spans.append((condition, body_start, body_end))
        position = skip_space(text, body_end)
        if not keyword_at(text, position, 'else'):
            return arms, position
        position = skip_space(text, position + len('else'))
        if keyword_at(text, position, 'if'):
            continue
        body_end = statement_end(text, position)
        arms.append(('', line_of(text, position, first_line), text[position:body_end],
                     line_of(text, position, first_line)))
        spans.append(('', position, body_end))
        return arms, body_end


CASE_LABEL = re.compile(r'\b(case\s+([^:;]+?)|default)\s*:(?!:)')


def parse_switch(text, position, first_line, spans=None):
    """The case groups of the switch starting at `position` (at 'switch'): consecutive labels share
    one body. Returns [(labels, label_line, body, body_line)] and the end position; `spans`
    (optional) gets (labels, body start, body end) per group."""
    open_brace = text.index('{', position)
    close_brace = matching(text, open_brace, '{', '}')
    inner = text[open_brace + 1:close_brace - 1]
    inner_line = line_of(text, open_brace + 1, first_line)
    labels = []
    depth = 0
    index = 0
    while index < len(inner):
        char = inner[index]
        if char == '{':
            depth += 1
        elif char == '}':
            depth -= 1
        elif depth == 0 and inner[index] in 'cd':
            match = CASE_LABEL.match(inner, index)
            if match and (index == 0 or not (inner[index - 1].isalnum() or inner[index - 1] == '_')):
                labels.append((match.start(), match.end(), (match.group(2) or 'default').strip()))
                index = match.end()
                continue
        index += 1
    groups = []
    current = []
    for number, (start, end, label) in enumerate(labels):
        current.append((label, line_of(inner, start, inner_line)))
        next_start = labels[number + 1][0] if number + 1 < len(labels) else len(inner)
        body = inner[end:next_start]
        if body.strip():
            groups.append(([name for name, _ in current], current[0][1], body, line_of(inner, end, inner_line)))
            if spans is not None:
                spans.append(([name for name, _ in current], open_brace + 1 + end, open_brace + 1 + next_start))
            current = []
    return groups, close_brace


def find_after(text, needles, start=0):
    """Position of the last needle, each searched after the previous one."""
    position = start
    for needle in needles:
        found = text.find(needle, position)
        if found < 0:
            raise CodeError(f'{needle!r} not found')
        position = found
    return position


def anchor_line(text, needles):
    return text.count('\n', 0, find_after(text, needles)) + 1


def operand_value(operand, symbols):
    cleaned = operand.replace('static_cast<int>(', '').replace(')', '')
    return evaluate(tokenize(cleaned), symbols.value)


def condition_types(condition, symbols):
    """Model type values selected by a branch condition, the rest of the condition, and the
    operands that could not be resolved. Ranges include both ends as the operator says."""
    text = ' '.join(condition.split())
    types, unresolved = [], []
    rest = text
    for pattern, low_group, high_group, op_group in RANGES:
        for match in pattern.finditer(text):
            try:
                low = operand_value(match.group(low_group), symbols)
                high = operand_value(match.group(high_group), symbols)
            except Unresolved:
                unresolved.append(match.group(0))
                continue
            inclusive = match.group(op_group) in ('<=', '>=')
            types.extend(range(low, high + 1 if inclusive else high))
            rest = rest.replace(match.group(0), ' ')
    for pattern in EQUALS:
        for match in pattern.finditer(rest):
            try:
                types.append(operand_value(match.group(1), symbols))
            except Unresolved:
                unresolved.append(match.group(0))
                continue
            rest = rest.replace(match.group(0), ' ')
    for ignored in IGNORED_QUALIFIERS:
        rest = rest.replace(ignored, ' ')
    leftover = re.sub(r'[()&|!\s]+', ' ', rest).strip()
    qualifiers = [leftover] if leftover else []
    return sorted(set(types)), qualifiers, unresolved


def split_args(text):
    args, depth, current = [], 0, []
    for char in text:
        if char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
        if char == ',' and depth == 0:
            args.append(''.join(current).strip())
            current = []
        else:
            current.append(char)
    tail = ''.join(current).strip()
    if tail:
        args.append(tail)
    return args


def nesting_ranges(body):
    """Character ranges of the body that sit inside a nested control statement."""
    ranges = []
    for match in CONTROL.finditer(body):
        word = match.group(1)
        if word in ('case', 'default'):
            continue
        position = match.end()
        if word in ('if', 'for', 'while', 'switch'):
            open_paren = body.find('(', position)
            if open_paren < 0:
                continue
            try:
                position = matching(body, open_paren, '(', ')')
            except CodeError:
                continue
        try:
            end = statement_end(body, position)
        except CodeError:
            continue
        ranges.append((match.start(), end))
    return ranges


def inside(position, ranges):
    return any(start <= position < end for start, end in ranges)


def body_facts(body, body_line):
    """Calls, assignments, Level changes and whether the body returns."""
    nested = nesting_ranges(body)
    calls, assignments, level = [], [], []
    for match in CALL.finditer(body):
        close = matching(body, match.end() - 1, '(', ')')
        args = split_args(body[match.end():close - 1])
        calls.append(Call(match.group(1), args, line_of(body, match.start(), body_line),
                          inside(match.start(), nested), body[match.start():close], match.start()))
    for match in ASSIGN.finditer(body):
        target = match.group(1) or LOOK_FIELDS[match.group(2)]
        assignments.append(Assignment(target, match.group(3).strip(), line_of(body, match.start(), body_line),
                                      inside(match.start(), nested), match.start()))
    for match in LEVEL_ASSIGN.finditer(body):
        level.append((match.group(1), match.group(2).strip(), line_of(body, match.start(), body_line), match.start()))
    return calls, assignments, level, RETURN.search(body) is not None


def make_branch(function, condition, line, body, body_line, symbols, labels=None):
    if labels is not None:
        types, qualifiers, unresolved = [], [], []
        for label in labels:
            if label == 'default':
                continue
            try:
                types.append(operand_value(label, symbols))
            except Unresolved:
                unresolved.append(label)
        condition = ' | '.join(labels)
    else:
        types, qualifiers, unresolved = condition_types(condition, symbols) if condition else ([], [], [])
    calls, assignments, level, returns = body_facts(body, body_line)
    return Branch(function, line, condition, types, qualifiers, unresolved, calls, assignments, level, returns,
                  body_line, body)


def chain_branches(function, text, first_line, needles, symbols):
    """The arms of the if-chain that starts at the last of `needles` (which must point at 'if')."""
    position = find_after(text, needles)
    arms, _ = parse_if_chain(text, position, first_line)
    return [make_branch(function, condition, line, body, body_line, symbols)
            for condition, line, body, body_line in arms]


def switch_branches(function, text, first_line, needles, symbols):
    """The case groups of the switch that starts at the last of `needles` (at 'switch')."""
    position = find_after(text, needles)
    groups, _ = parse_switch(text, position, first_line)
    return [make_branch(function, '', line, body, body_line, symbols, labels=labels)
            for labels, line, body, body_line in groups]


def sequence_branches(function, text, first_line, name, symbols):
    """The ifs in a row at the top level of the function `name`, each an arm of its own (and its
    else-ifs, if it has any): the form of a function that returns from each of them."""
    body, body_line = function_body(text, name)
    if body is None:
        raise CodeError(f'function {name} not found')
    branches, position = [], 0
    while True:
        found = next_top_level_if(body, position)
        if found is None:
            return branches
        arms, position = parse_if_chain(body, found, body_line)
        branches += [make_branch(function, condition, line, arm_body, arm_line, symbols)
                     for condition, line, arm_body, arm_line in arms]


def next_top_level_if(text, start):
    """Position of the next `if` outside every bracket at or after `start`, or None."""
    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char in '({[':
            depth += 1
        elif char in ')}]':
            depth -= 1
        elif depth == 0 and char == 'i' and keyword_at(text, index, 'if') and \
                (index == 0 or not (text[index - 1].isalnum() or text[index - 1] == '_')):
            return index
    return None


def function_branch(function, text, symbols):
    """The whole body of a function as one branch (a named render style or item effect)."""
    body, line = function_body(text, function)
    if body is None:
        raise CodeError(f'function {function} not found')
    return make_branch(function, '', line, body, line, symbols)


TABLE_ENTRY = re.compile(r'\{\s*"(\w+)"\s*((?:,[^{}]*)?)\}')


def name_table(text, needle):
    """The rows of a table of named functions that starts at `needle` (`const X Table[] = {`):
    [(name, [the other values of the row], line)], in the table's order."""
    start = find_after(text, (needle,))
    open_brace = text.index('{', start + len(needle) - 1)
    close_brace = matching(text, open_brace, '{', '}')
    rows = []
    for match in TABLE_ENTRY.finditer(text, open_brace + 1, close_brace - 1):
        values = [value.strip() for value in match.group(2).split(',')[1:]]
        rows.append((match.group(1), values, text.count('\n', 0, match.start()) + 1))
    if not rows:
        raise CodeError(f'{needle!r} has no rows')
    return rows


LOOP = re.compile(r'\b(for|while|switch)\s*\(')
IF_WORD = re.compile(r'\bif\s*\(')
ELSE_BEFORE = re.compile(r'\belse\s*$')
# for (int i = 1; i < b->NumMeshs; ++i): one pass per mesh from a first mesh on.
MESH_LOOP = re.compile(r'int\s+(\w+)\s*=\s*(\d+)\s*;\s*\1\s*<\s*b->NumMeshs\s*;\s*(?:\+\+\1|\1\+\+)')
TYPE_SWITCH = ('Type', 'o->Type', 'w->Type')
CASE_END = re.compile(r'\b(?:break|return\b[^;]*)\s*;\s*\}?\s*$')


def enclosing_loops(body, position):
    """(word, head, start) of every for / while / switch statement around `position`."""
    found = []
    for match in LOOP.finditer(body):
        try:
            open_paren = body.index('(', match.start())
            close_paren = matching(body, open_paren, '(', ')')
            end = statement_end(body, close_paren)
        except CodeError:
            continue
        if match.start() < position < end:
            found.append((match.group(1), body[open_paren + 1:close_paren - 1], match.start()))
    return found


def mesh_loop(body, position):
    """(loop variable, first mesh) when the statement at `position` sits in a loop over the model's
    meshes (MESH_LOOP) and in no other loop; else None."""
    loops = [(word, head) for word, head, _ in enclosing_loops(body, position) if word != 'switch']
    if len(loops) != 1 or loops[0][0] != 'for':
        return None
    match = MESH_LOOP.fullmatch(' '.join(loops[0][1].split()))
    return (match.group(1), int(match.group(2))) if match else None


def switch_arm(body, start, position, value, symbols):
    """Whether the case group around `position` of the switch at `start` runs for the model type
    `value`: True, False, or None when a case falls through or a label cannot be read."""
    spans = []
    try:
        parse_switch(body, start, 1, spans)
    except (CodeError, ValueError):
        return None
    selected, default, around = None, None, None
    for number, (labels, group_start, group_end) in enumerate(spans):
        if not CASE_END.search(body[group_start:group_end]):
            return None
        if group_start <= position < group_end:
            around = number
        for label in labels:
            if label == 'default':
                default = number
                continue
            try:
                if operand_value(label, symbols) == value:
                    selected = number
            except Unresolved:
                return None
    if around is None:
        return None
    return around == (default if selected is None else selected)


def guard(body, position, decide, value=None, symbols=None):
    """Whether the statement at `position` of a branch body runs: True, False, or None when it
    depends on something `decide` cannot say (a buff, the light, a loop). decide(condition) says
    whether an `if` condition holds: True, False or None. A loop over the model's meshes
    (mesh_loop) and a switch over the model type (with `value` and `symbols`) are decided too."""
    decided = True
    for word, head, start in enclosing_loops(body, position):
        if word == 'for' and MESH_LOOP.fullmatch(' '.join(head.split())):
            continue
        taken = None
        if word == 'switch' and head.strip() in TYPE_SWITCH and value is not None:
            taken = switch_arm(body, start, position, value, symbols)
        if taken is False:
            return False
        if taken is None:
            decided = None
    for match in IF_WORD.finditer(body):
        if match.start() >= position or ELSE_BEFORE.search(body[:match.start()]):
            continue
        spans = []
        try:
            parse_if_chain(body, match.start(), 1, spans)
        except CodeError:
            continue
        for number, (condition, start, end) in enumerate(spans):
            if not start <= position < end:
                continue
            taken = arm_taken(spans[:number + 1], decide)
            if taken is False:
                return False
            if taken is None:
                decided = None
    return decided


def enclosing_condition(body, position):
    """The condition of the innermost `if` arm around `position`, or '' when there is none."""
    found, size = '', None
    for match in IF_WORD.finditer(body):
        if match.start() >= position:
            continue
        spans = []
        try:
            parse_if_chain(body, match.start(), 1, spans)
        except CodeError:
            continue
        for condition, start, end in spans:
            if start <= position < end and (size is None or end - start < size):
                found, size = ' '.join(condition.split()), end - start
    return found


def type_decider(value, symbols):
    """decide() for conditions that only compare the model type."""
    def decide(condition):
        types, qualifiers, unresolved = condition_types(condition, symbols)
        if qualifiers or unresolved or not types:
            return None
        return value in types
    return decide


def type_guard(body, position, value, symbols):
    """Whether the statement at `position` of a branch body runs when the model type is `value`:
    True, False, or None when it depends on something else than the type (a buff, the light, a
    loop). Nested if-chains whose conditions only compare the type are decided."""
    return guard(body, position, type_decider(value, symbols))


def arm_taken(arms, decide):
    """The last of `arms` runs when no earlier arm's condition holds and its own does."""
    for number, (condition, _, _) in enumerate(arms):
        last = number == len(arms) - 1
        if not condition:
            return True if last else None
        holds = decide(condition)
        if holds is None:
            return None
        if holds:
            return last
    return False
