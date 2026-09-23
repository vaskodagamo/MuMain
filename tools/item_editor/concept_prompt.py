"""Build the concept prompt of one item from concept_prompt.md; standard library only.

The template is owned by the owner: plain text sections under `## <name>` headings, with
`$placeholders` (string.Template). The tier palette and material rules come from the art study's
style guide (assets-work/Items/study/baseline.json), so the prompt follows the study's T1-T7 plan.
A refine prompt (a new round from one picked variant plus the owner's comment) is built from the
`## refine` section instead of `## base` and the variant hints.
"""

from pathlib import Path
from string import Template
import json

import concept_select

HERE = Path(__file__).resolve().parent
TEMPLATE_FILE = HERE / 'concept_prompt.md'
SECTION_PREFIX = '## '
SECTION_BASE = 'base'
SECTION_NOTE = 'note'
SECTION_REFINE = 'refine'
FAMILY_DEFAULT = 'default'
HINTS_SAME = 'same'
HINTS_DISTINCT = 'distinct'
VIEW_SINGLE = 'single'
VIEW_SHEET = 'sheet'
PALETTE_ORDER = ('metal', 'base', 'leather', 'accent', 'glow')


class PromptError(ValueError):
    pass


def read_sections(path=TEMPLATE_FILE):
    """concept_prompt.md -> {section name: text}; text above the first heading is ignored."""
    sections = {}
    current = None
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        if line.startswith(SECTION_PREFIX):
            current = line[len(SECTION_PREFIX):].strip()
            sections[current] = []
        elif current is not None:
            sections[current].append(line)
    return {name: '\n'.join(lines).strip() for name, lines in sections.items()}


def style_facts(baseline):
    """baseline.json style_guide -> {tier: (role, palette text)}, materials text."""
    guide = baseline['style_guide']
    tiers = {}
    for row in guide['palette_by_tier']:
        colours = row['colors']
        palette = ', '.join(f'{name} {colours[name]}' for name in PALETTE_ORDER if name in colours)
        tiers[int(row['tier'].lstrip('T'))] = (row['role'], palette)
    materials = ' '.join(f'{row["material"]}: {row["treatment"]}' for row in guide['materials'])
    return tiers, materials


def load_style(baseline_path=concept_select.BASELINE_FILE):
    return style_facts(json.loads(Path(baseline_path).read_text(encoding='utf-8')))


def section(sections, name):
    if name not in sections:
        raise PromptError(f'{TEMPLATE_FILE.name} has no "## {name}" section')
    return sections[name]


def variant_section(sections, hints, variant):
    if hints == HINTS_SAME:
        return section(sections, f'variant {HINTS_SAME}')
    return section(sections, f'variant {variant}')


def distinct_hint_count(sections):
    count = 0
    while f'variant {count + 1}' in sections:
        count += 1
    return count


def family_part(sections, subject):
    return sections.get(f'family {subject["family"]}') or section(sections, f'family {FAMILY_DEFAULT}')


def prompt_values(style, subject, note):
    tiers, materials = style
    role, palette = tiers[subject['tier']]
    return {
        'name': subject['name'], 'keys': ', '.join(subject['keys']),
        'family': concept_select.family_label(subject['family']), 'tier': subject['tier'],
        'tier_role': role, 'palette': palette, 'materials': materials, 'note': note or '',
    }


def substitute(parts, values):
    try:
        return '\n\n'.join(Template(part).substitute(values) for part in parts)
    except (KeyError, ValueError) as error:
        raise PromptError(f'{TEMPLATE_FILE.name}: bad placeholder {error}') from None


def build_prompt(sections, style, subject, view, hints, variant=1, note=None):
    """The prompt text of one request (one variant hint) for one subject."""
    parts = [
        section(sections, SECTION_BASE),
        family_part(sections, subject),
        section(sections, f'view {view}'),
        variant_section(sections, hints, variant),
    ]
    if note:
        parts.append(section(sections, SECTION_NOTE))
    return substitute(parts, prompt_values(style, subject, note))


def build_refine_prompt(sections, style, subject, view, note):
    """The prompt of a refine request: revise the attached concept as the owner's comment says."""
    if not note:
        raise PromptError('a refine needs the owner\'s comment (--note)')
    parts = [section(sections, SECTION_REFINE), family_part(sections, subject), section(sections, f'view {view}')]
    return substitute(parts, prompt_values(style, subject, note))
