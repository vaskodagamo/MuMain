"""Human-readable output of concepts.py (plans, cost splits, batch costs); standard library only.

Everything here prints with print(): on stdout in text mode, on stderr in the JSON modes
(concept_events.Reporter.human_output).
"""

from pathlib import Path

import concept_batch
import concept_cost
import concept_refs

PROMPT_INDENT = '      '


def money(value):
    return f'${value:.4f}' if value < 1 else f'${value:.2f}'


def cost_split(cost):
    parts = cost['parts']
    return (f'text {money(parts["text"])} + reference {money(parts["reference"])} + '
            f'output {money(parts["output"])} = {money(cost["total"])}')


def display_path(path, root):
    path = Path(path)
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def settings_line(settings):
    return (f'preset {settings["preset"]}: {settings["model"]}, quality {settings["quality"]}, {settings["size"]}, '
            f'background {settings["background"]}, {settings["variants"]} image(s) per item '
            f'(hints {settings["hints"]}), reference {settings["ref_size"]} px')


def print_subject(subject, refs_dir, show_prompts, root):
    rank = f', study rank {subject["study_rank"]}' if subject.get('study_rank') else ''
    print(f'\n{subject["key"]}  {subject["name"]}  ({subject["family"]}, T{subject["tier"]}{rank})')
    if len(subject['keys']) > 1:
        print(f'  covers: {", ".join(subject["keys"])}')
    if subject.get('parent'):
        parent = subject['parent']
        print(f'  refines: {parent["batch"]}/{parent["key"]}/{parent["variant"]}  ({subject["note"]})')
    else:
        reference = concept_refs.ref_path(refs_dir, subject['key'])
        state = '' if reference.is_file() else f'  MISSING: run concepts.py refs --keys {subject["key"]}'
        print(f'  reference: {display_path(reference, root)}{state}')
    for request in subject['requests']:
        variants = ', '.join(f'v{n}' for n in request['variants'])
        print(f'  {request["id"]} (n={request["n"]}: {variants}): {cost_split(request["estimate"])}')
        if show_prompts:
            print(PROMPT_INDENT + request['prompt'].replace('\n', '\n' + PROMPT_INDENT))


def print_totals(total, max_images, max_cost):
    print(f'\nTotal (estimate): {total["images"]} images in {total["requests"]} requests: {cost_split(total)}')
    for flag in total['flags']:
        print(f'  flag: {flag}')
    problems = concept_cost.cap_problems(total['images'], total['total'], max_images, max_cost)
    caps = f'--max-images {max_images}, --max-cost {money(max_cost)}'
    print(f'Caps ({caps}): ' + ('; '.join(problems) if problems else 'within both'))
    return problems


def print_batch_costs(batch):
    print(f'Batch {batch["batch"]}: {settings_line(batch["settings"])}')
    estimated, actual = [], []
    for subject in batch['subjects']:
        for request in subject['requests']:
            result = request['result'] or {}
            estimated.append(request['estimate'])
            line = f'{subject["key"]}/{request["id"]}: estimated {money(request["estimate"]["total"])}'
            if result.get('actual'):
                actual.append(result['actual'])
                difference = result['actual']['total'] - request['estimate']['total']
                line += f', actual {cost_split(result["actual"])} (difference {difference:+.4f})'
            elif result.get('error'):
                line += f', failed: {result["error"]}'
            print('  ' + line)
    print_cost_comparison(estimated, actual)


def print_cost_comparison(estimated, actual):
    total_estimate = concept_batch.sum_costs(estimated)
    print(f'Estimated: {cost_split(total_estimate)}')
    if not actual:
        print('Actual: no usage recorded yet')
        return
    total_actual = concept_batch.sum_costs(actual)
    print(f'Actual (from usage, {len(actual)} of {len(estimated)} requests): {cost_split(total_actual)}')
    for part in concept_batch.COST_PARTS + ('total',):
        guess = total_estimate['total'] if part == 'total' else total_estimate['parts'][part]
        real = total_actual['total'] if part == 'total' else total_actual['parts'][part]
        print(f'  {part:9} estimated {money(guess)}  actual {money(real)}  difference {real - guess:+.4f}')
