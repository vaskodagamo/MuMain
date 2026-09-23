#!/usr/bin/env python3
"""Generate tools/item_editor/item_models.json: which model files the client loads for each item.

The client has no data file for this; the mapping is C++ code. This script runs that code
symbolically (cpp_source.py / cpp_runner.py) and writes the result:

- OpenPlayers, OpenPlayerTextures, OpenItems, OpenItemTextures (Engine/Object/ZzzOpenData.cpp, in
  the order OpenBasicData calls them), including the member calls they make
  (g_ChangeRingMgr->LoadItemModel(), g_CMonkSystem.LoadModelItem(), ...). Every
  gLoadData.AccessModel(type, dir, name, i) names the file dir + name + NN + ".bmd" exactly like
  CLoadData::AccessModel (LoadData.cpp): i == -1 -> "<name>.bmd", i < 10 -> "<name>0<i>.bmd", else
  "<name><i>.bmd". A later load of the same model type replaces an earlier one. Every
  gLoadData.OpenTexture(type, subfolder) adds Data/<subfolder> as a folder the model's textures are
  read from.
- Types MODEL_ITEM .. MODEL_ITEM + MAX_ITEM - 1 are items (type - MODEL_ITEM = group * 512 + index).
- Models outside that range are linked to an item where the engine swaps them in (per-class
  armour replacements, Rage Fighter hand models, inventory models; see class_variant_links,
  hand_model_links and inventory_links); the rest are listed under other_models.
- The hard-coded socket item list of CSocketItemMgr::IsSocketItem(int) (SocketSystem.cpp).

Paths are matched against src/bin/Data case-insensitively and written with the on-disk spelling;
files that do not exist are listed under missing_files instead of being dropped.

Usage (from anywhere):
    python3 tools/item_editor/gen_item_models.py           # write item_models.json
    python3 tools/item_editor/gen_item_models.py --check   # exit 1 when it is out of date
"""

import argparse
import json
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from cpp_runner import Runner  # noqa: E402
from cpp_source import (SymbolTable, Unresolved, defined_macros, evaluate, function_body,  # noqa: E402
                        read_source, strip_comments, tokenize)

ROOT = HERE.parents[1]
OUTPUT = HERE / 'item_models.json'
SCHEMA = 'mu-item-models/1'

GLOBAL_HEADERS = ('Core/Globals/_define.h', 'Core/Globals/_enum.h')
OPEN_DATA = 'Engine/Object/ZzzOpenData.cpp'
LOAD_FUNCTIONS = ('OpenPlayers', 'OpenPlayerTextures', 'OpenItems', 'OpenItemTextures')
SOCKET_FILE = 'Network/Server/SocketSystem.cpp'
SOCKET_FUNCTION = re.compile(r'IsSocketItem\s*\(\s*int\s+\w+\s*\)\s*\{')
MONK_FILE = 'GameLogic/Social/MonkSystem.cpp'
SET_MODEL_TYPE = re.compile(r'SetModelType\s*\(([^;]*)\)\s*;')
INVENTORY_FILE = 'Engine/Object/ZzzInventory.cpp'
INVENTORY_FUNCTION = 'RenderItem3D'
RENDER_SCREEN = re.compile(r'RenderObjectScreen\s*\(\s*([^,]+),')
CONDITION = re.compile(r'\b(?:else\s+)?if\s*\((.*)\)\s*$')
TYPE_EQUALS = re.compile(r'Type\s*==\s*([A-Za-z_]\w*(?:\s*\+\s*\d+)?)')
TYPE_RANGE = re.compile(r'Type\s*>=\s*([A-Za-z_]\w*(?:\s*\+\s*\d+)?)\s*&&\s*Type\s*<=\s*([A-Za-z_]\w*(?:\s*\+\s*\d+)?)')
DEFAULT_INVENTORY_MODEL = 'Type + MODEL_ITEM'

ACCESS_MODEL = 'AccessModel'
OPEN_TEXTURE = 'OpenTexture'
NO_INDEX = -1
TWO_DIGITS = 10
DATA_PREFIX = 'Data/'
MODEL_PREFIX = 'MODEL_'

# Class-specific replacement models the engine renders instead of an armour item's own model:
#   Summoner (ZzzCharacter.cpp, RenderCharacter): helm..boots index 10 and 11 ->
#       MODEL_HELM2 + (group - 7) * MODEL_ITEM_COMMON_NUM + (index - 10)
#   Rage Fighter (MonkSystem.cpp, ModifyTypeCommonItemMonk): indexes 5, 6, 8, 9 ->
#       MODEL_HELM_MONK + (group' - 7) * MODEL_ITEM_COMMONCNT_RAGEFIGHTER + position, where
#       group' is 10 for boots (group 11)
#   Dark Lord (ZzzCharacter.cpp, RenderCharacter): helm index 0, 5, 6, 8, 9 -> MODEL_MASK_HELM + index
ARMOUR_GROUPS = range(7, 12)
HELM_GROUP = 7
BOOTS_GROUP = 11
GLOVES_SLOT_FOR_MONK = 10
SUMMONER_INDEXES = (10, 11)
RAGE_FIGHTER_INDEXES = (5, 6, 8, 9)
DARK_LORD_MASK_INDEXES = (0, 5, 6, 8, 9)


class Generator:
    def __init__(self, root=ROOT):
        self.root = root
        self.source = root / 'src' / 'source'
        self.data_parent = root / 'src' / 'bin'
        self.consulted = {}
        header_texts = [strip_comments(p.read_text(encoding='utf-8', errors='replace'))
                        for p in sorted(self.source.rglob('*.h'))]
        self.macros = defined_macros(header_texts)
        self.symbols = SymbolTable()
        for header in GLOBAL_HEADERS:
            self.symbols.add_header(read_source(self.source / header, self.macros, self.consulted))
        self.model_item = self.symbols.value('MODEL_ITEM')
        self.max_item = self.symbols.value('MAX_ITEM')
        self.max_index = self.symbols.value('MAX_ITEM_INDEX')
        self.sources = {}
        self.methods = None
        self.disk = {}

    # --- source access -------------------------------------------------------------------

    def text(self, relative):
        if relative not in self.sources:
            self.sources[relative] = read_source(self.source / relative, self.macros, self.consulted)
        return self.sources[relative]

    def method_index(self):
        """Method name -> [(file, Class::Method)] for every argument-less member function."""
        if self.methods is None:
            self.methods = {}
            pattern = re.compile(r'\bvoid\s+(\w+)::(\w+)\s*\(\s*\)\s*\{')
            for path in sorted(self.source.rglob('*.cpp')):
                raw = path.read_text(encoding='utf-8', errors='replace')
                for match in pattern.finditer(raw):
                    relative = path.relative_to(self.source).as_posix()
                    self.methods.setdefault(match.group(2), []).append((relative, f'{match.group(1)}::{match.group(2)}'))
        return self.methods

    def resolve_method(self, name):
        found = self.method_index().get(name, [])
        if len(found) != 1:
            return None
        relative, qualified = found[0]
        body, line = function_body(self.text(relative), qualified)
        return (qualified, body, line) if body is not None else None

    # --- running the load code -----------------------------------------------------------

    def run_loads(self):
        runner = Runner(self.symbols, (ACCESS_MODEL, OPEN_TEXTURE), self.resolve_method)
        for function in LOAD_FUNCTIONS:
            body, line = function_body(self.text(OPEN_DATA), function)
            if body is None:
                raise Unresolved(f'{OPEN_DATA}: function {function} not found')
            runner.run(function, body, line)
        return runner

    def collect_models(self, runner):
        """Model type -> {'load', 'earlier', 'texture_dirs'} in engine order."""
        models = {}
        for call in runner.calls:
            model = models.setdefault(call.args[0], {'load': None, 'earlier': [], 'texture_dirs': []})
            if call.name == ACCESS_MODEL:
                if model['load'] is not None:
                    model['earlier'].append(model['load'])
                model['load'] = self.model_file(call)
            else:
                folder = DATA_PREFIX + call.args[1].replace('\\', '/')
                if folder not in model['texture_dirs']:
                    model['texture_dirs'].append(folder)
        return models

    def model_file(self, call):
        directory, name = call.args[1], call.args[2]
        index = call.args[3] if len(call.args) > 3 else NO_INDEX
        if index == NO_INDEX:
            file = f'{name}.bmd'
        elif index < TWO_DIGITS:
            file = f'{name}0{index}.bmd'
        else:
            file = f'{name}{index}.bmd'
        referenced = directory.replace('\\', '/') + file
        return {'referenced': referenced, 'loaded_by': call.function}

    # --- files on disk -------------------------------------------------------------------

    def on_disk(self, relative):
        """Repo path with the on-disk spelling of every component, or None when it does not exist."""
        current, parts = self.data_parent, relative.strip('/').split('/')
        spelled = []
        for part in parts:
            if current not in self.disk:
                self.disk[current] = {p.name.lower(): p.name for p in current.iterdir()} if current.is_dir() else {}
            name = self.disk[current].get(part.lower())
            if name is None:
                return None
            spelled.append(name)
            current = current / name
        return 'src/bin/' + '/'.join(spelled)

    def file_entry(self, load):
        path = self.on_disk(load['referenced'])
        entry = {'path': path or 'src/bin/' + load['referenced'], 'exists': path is not None,
                 'loaded_by': load['loaded_by']}
        return entry

    def folder_entries(self, folders):
        return [self.on_disk(folder) or 'src/bin/' + folder for folder in folders]

    # --- items and extra models ----------------------------------------------------------

    def item_key(self, model_type):
        group, index = divmod(model_type - self.model_item, self.max_index)
        return f'{group}-{index}', group, index

    def is_item(self, model_type):
        return self.model_item <= model_type < self.model_item + self.max_item

    def model_record(self, model):
        record = {'file': self.file_entry(model['load']), 'texture_dirs': self.folder_entries(model['texture_dirs'])}
        if model['earlier']:
            record['replaces'] = sorted({load['referenced'] for load in model['earlier']})
        return record

    def symbol_name(self, model_type):
        """A readable MODEL_ name for a model type: exact name, else the nearest name below + offset."""
        exact, below = [], None
        for name in sorted(self.symbols.definitions):
            if not name.startswith(MODEL_PREFIX):
                continue
            try:
                value = self.symbols.value(name)
            except Unresolved:
                continue
            if value == model_type:
                exact.append(name)
            elif value < model_type and (below is None or value > below[1]):
                below = (name, value)
        if exact:
            return exact[0]
        return f'{below[0]} + {model_type - below[1]}' if below else str(model_type)

    def class_variant_links(self):
        """(item type, model type, role) for the per-class armour replacements (see ARMOUR rules)."""
        value = self.symbols.value
        links = []
        for group in ARMOUR_GROUPS:
            base = group * self.max_index
            for index in SUMMONER_INDEXES:
                model = value('MODEL_HELM2') + (group - HELM_GROUP) * value('MODEL_ITEM_COMMON_NUM') + index - SUMMONER_INDEXES[0]
                links.append((base + index, model, 'class-variant', 'sum'))
            monk_group = GLOVES_SLOT_FOR_MONK if group == BOOTS_GROUP else group
            for position, index in enumerate(RAGE_FIGHTER_INDEXES):
                model = value('MODEL_HELM_MONK') + (monk_group - HELM_GROUP) * value('MODEL_ITEM_COMMONCNT_RAGEFIGHTER') + position
                links.append((base + index, model, 'class-variant', 'rf'))
        for index in DARK_LORD_MASK_INDEXES:
            links.append((HELM_GROUP * self.max_index + index, value('MODEL_MASK_HELM') + index, 'class-variant', 'dl'))
        return links

    def hand_model_links(self):
        """MonkSystem SetModelType(item model, left hand model, right hand model)."""
        links = []
        for match in SET_MODEL_TYPE.finditer(self.text(MONK_FILE)):
            parts = [evaluate(tokenize(part), self.symbols.value) for part in match.group(1).split(',')]
            item_model, left, right = parts[:3]
            links.append((item_model - self.model_item, left, 'left-hand', None))
            links.append((item_model - self.model_item, right, 'right-hand', None))
        return links

    def inventory_links(self):
        """RenderItem3D (ZzzInventory.cpp): items drawn in the inventory with another model."""
        body, _ = function_body(self.text(INVENTORY_FILE), INVENTORY_FUNCTION)
        links, type_condition, inner_condition = [], None, None
        for line in body.split('\n'):
            found = CONDITION.search(line.strip())
            if found and 'Type' in found.group(1):
                type_condition, inner_condition = found.group(1).strip(), None
            elif found:
                inner_condition = found.group(1).strip()
            screen = RENDER_SCREEN.search(line)
            if not screen or type_condition is None or screen.group(1).strip() == DEFAULT_INVENTORY_MODEL:
                continue
            condition = type_condition if inner_condition is None else f'{type_condition}; {inner_condition}'
            model = evaluate(tokenize(screen.group(1)), self.symbols.value)
            for item_type in self.condition_items(type_condition):
                if model != self.model_item + item_type:
                    links.append((item_type, model, 'inventory', condition))
        return links

    def condition_items(self, condition):
        value = lambda text: evaluate(tokenize(text), self.symbols.value)  # noqa: E731
        ranged = TYPE_RANGE.search(condition)
        if ranged:
            return list(range(value(ranged.group(1)), value(ranged.group(2)) + 1))
        return [value(match.group(1)) for match in TYPE_EQUALS.finditer(condition)]

    def socket_items(self):
        text = self.text(SOCKET_FILE)
        match = SOCKET_FUNCTION.search(text)
        body_end = text.find('default:', match.end())
        cases = re.findall(r'case\s+([^:]+):', text[match.end():body_end])
        return sorted({evaluate(tokenize(case), self.symbols.value) for case in cases})

    # --- assembly ------------------------------------------------------------------------

    def build(self):
        runner = self.run_loads()
        models = self.collect_models(runner)
        items, other = {}, {}
        for model_type, model in models.items():
            if model['load'] is None:
                continue
            if self.is_item(model_type):
                key, group, index = self.item_key(model_type)
                items[key] = dict(self.model_record(model), group=group, index=index, extra_models=[])
            else:
                other[model_type] = model
        linked = set()
        for item_type, model_type, role, detail in (self.class_variant_links() + self.hand_model_links()
                                                    + self.inventory_links()):
            key = '{}-{}'.format(*divmod(item_type, self.max_index))
            model = models.get(model_type)
            if key not in items or model is None or model['load'] is None:
                continue
            extra = dict(self.model_record(model), role=role, model=self.symbol_name(model_type))
            if role == 'class-variant':
                extra['class'] = detail
            elif role == 'inventory':
                extra['condition'] = detail
            if extra not in items[key]['extra_models']:
                items[key]['extra_models'].append(extra)
            linked.add(model_type)
        return self.document(items, other, linked, runner)

    def document(self, items, other, linked, runner):
        for item in items.values():
            item['extra_models'].sort(key=lambda e: (e['role'], e.get('class') or '', e['file']['path'], e.get('condition', '')))
        other_models = [dict(self.model_record(model), model=self.symbol_name(model_type))
                        for model_type, model in sorted(other.items()) if model_type not in linked]
        referenced = [(key, item['file']) for key, item in items.items()]
        referenced += [(key, extra['file']) for key, item in items.items() for extra in item['extra_models']]
        missing = sorted({(key, entry['path']) for key, entry in referenced if not entry['exists']})
        return {
            'schema': SCHEMA,
            'generated_by': 'tools/item_editor/gen_item_models.py',
            'sources': sorted({f'src/source/{p}' for p in list(self.sources) + list(GLOBAL_HEADERS)}),
            'macros': dict(sorted(self.consulted.items())),
            'counts': {'items': len(items), 'extra_models': sum(len(i['extra_models']) for i in items.values()),
                       'other_models': len(other_models), 'missing_files': len(missing)},
            'items': items,
            'socket_items': ['{}-{}'.format(*divmod(t, self.max_index)) for t in self.socket_items()],
            'other_models': other_models,
            'missing_files': [{'item': key, 'path': path} for key, path in missing],
            'skipped_statements': runner.skipped,
        }


def render(document):
    return json.dumps(document, indent=1, sort_keys=True, ensure_ascii=False) + '\n'


def main(argv=None):
    parser = argparse.ArgumentParser(description='Generate tools/item_editor/item_models.json from the engine code.')
    parser.add_argument('--check', action='store_true', help='exit 1 when item_models.json is out of date')
    parser.add_argument('--output', type=Path, default=OUTPUT)
    args = parser.parse_args(argv)
    text = render(Generator().build())
    if args.check:
        current = args.output.read_text(encoding='utf-8') if args.output.exists() else ''
        if current != text:
            print(f'{args.output} is out of date; run tools/item_editor/gen_item_models.py')
            return 1
        print(f'{args.output} is current')
        return 0
    args.output.write_text(text, encoding='utf-8')
    document = json.loads(text)
    print(f'wrote {args.output}: {json.dumps(document["counts"])}')
    for entry in document['missing_files']:
        print(f'  missing: {entry["item"]} {entry["path"]}')
    for entry in document['skipped_statements']:
        print(f'  skipped in {entry["function"]}: {entry["statement"]} ({entry["reason"]})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
