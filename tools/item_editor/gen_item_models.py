#!/usr/bin/env python3
"""Generate tools/item_editor/item_models.json: which model files the client loads for each item.

The item models themselves are data: src/bin/Data/Items/Models/Group*.json names each item's
model file and texture folders, directly (`file`, `textureFolders`) or through a shared model
(`model`, an entry of SharedModels.json); see docs/item-data.md, "Item models". The models that
are not items (bodies, per-class armour replacements, hand models) are still loaded by C++ code,
which this script runs symbolically (cpp_source.py / cpp_runner.py):

- OpenPlayers, OpenPlayerTextures, OpenItems, OpenItemTextures (Engine/Object/ZzzOpenData.cpp, in
  the order OpenBasicData calls them), including the functions of that file they call
  (OpenItemEffectModels(), OpenItemEffectModelTextures(): the event models and the Rage Fighter
  parts) and the member calls they make (g_CMonkSystem.LoadModelItem(), ...). Every
  gLoadData.AccessModel(type, dir, name, i) names the file dir + name + NN + ".bmd" exactly like
  CLoadData::AccessModel (LoadData.cpp): i == -1 -> "<name>.bmd", i < 10 -> "<name>0<i>.bmd", else
  "<name><i>.bmd". A later load of the same model type replaces an earlier one. Every
  gLoadData.OpenTexture(type, subfolder) adds Data/<subfolder> as a folder the model's textures are
  read from.
- The unequipped body parts (MODEL_BODY_HELM ... MODEL_BODY_BOOTS, one per class form) are opened by
  Engine/Object/PlayerBodyModels.cpp from a table of file prefixes; body_models() follows its rules
  (checked against the code's own lines) for the original Season 6 files in Data/Player, the default
  ("S6 player models" on; docs/player-base-models.md). The improved set in Data/Player/Updated that
  replaces them when that option is off is not listed.
- Types MODEL_ITEM .. MODEL_ITEM + MAX_ITEM - 1 are items (type - MODEL_ITEM = group * 512 + index).
- Models outside that range are linked to an item where the engine swaps them in (per-class
  armour replacements, Rage Fighter hand models, inventory models; see class_variant_links,
  hand_model_links and inventory_links); the rest are listed under other_models.
- The inventory models come from Render/Items/ItemDisplay.cpp: the ArmorInventoryModels table (the
  Rage Fighter armour) and FindInventoryModel, which is evaluated for every item at every level
  0..15 (level variants drawn with event models or with another item's model).
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
import render_code  # noqa: E402
from cpp_source import (SymbolTable, Unresolved, defined_macros, evaluate, function_body,  # noqa: E402
                        read_source, strip_comments, tokenize)

ROOT = HERE.parents[1]
OUTPUT = HERE / 'item_models.json'
SCHEMA = 'mu-item-models/1'

GLOBAL_HEADERS = ('Core/Globals/_define.h', 'Core/Globals/_enum.h')
ITEM_MODEL_DATA = 'Data/Items/Models'
ITEM_MODEL_FILES = 'Group*.json'
SHARED_MODEL_FILE = 'SharedModels.json'
OPEN_DATA = 'Engine/Object/ZzzOpenData.cpp'
LOAD_FUNCTIONS = ('OpenPlayers', 'OpenPlayerTextures', 'OpenItems', 'OpenItemTextures')
SOCKET_FILE = 'Network/Server/SocketSystem.cpp'
SOCKET_FUNCTION = re.compile(r'IsSocketItem\s*\(\s*int\s+\w+\s*\)\s*\{')
MONK_FILE = 'GameLogic/Social/MonkSystem.cpp'
SET_MODEL_TYPE = re.compile(r'SetModelType\s*\(([^;]*)\)\s*;')
# Render/Items/ItemDisplay.cpp: which model the inventory draws for an item at a level.
INVENTORY_FILE = 'Render/Items/ItemDisplay.cpp'
INVENTORY_FUNCTION = 'FindInventoryModel'
INVENTORY_SWITCH = 'switch (itemType)'
ARMOR_INVENTORY_TABLE = re.compile(r'ArmorInventoryModels\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;', re.S)
ARMOR_INVENTORY_ENTRY = re.compile(r'\{\s*([A-Za-z_]\w*(?:\s*\+\s*\d+)?)\s*,\s*([A-Za-z_]\w*(?:\s*\+\s*\d+)?)\s*\}')
# The function's own words, rewritten to an expression the evaluator reads: both lambdas return
# their model, `nothing` is no model, ToModelSlot is MODEL_ITEM + item type (ItemModelSlots.h).
INVENTORY_LAMBDAS = ('drawn', 'levelMinusOne')
INVENTORY_LAMBDA = re.compile(r'const\s+auto\s+(\w+)\s*=\s*\[[^\]]*\]\s*\(int\s+modelType\)\s*'
                              r'\{\s*return\s+std::optional<InventoryModel>\(InventoryModel\{modelType,')
INVENTORY_NOTHING = 'nothing'
INVENTORY_NO_MODEL = -1
INVENTORY_REWRITES = (('Data::Items::ToModelSlot(itemType)', '(MODEL_ITEM + itemType)'),
                      ('GetInventoryModel(itemType)', 'inventoryModel'))
INVENTORY_FALLBACK = 'return drawn(GetInventoryModel(itemType));'
INVENTORY_IF = re.compile(r'if\s*\(')
INVENTORY_RETURN = re.compile(r'return\b(.*)$', re.S)
ITEM_TYPE_WORD = re.compile(r'\bitemType\b')
ITEM_LEVELS = range(16)  # item levels +0..+15

# Engine/Object/PlayerBodyModels: the unequipped body parts of every class form.
BODY_HEADER = 'Engine/Object/PlayerBodyModels.h'
BODY_FILE = 'Engine/Object/PlayerBodyModels.cpp'
BODY_PARTS = re.compile(r'PlayerBodyParts\s*\{\{(.*?)\}\}\s*;', re.S)
BODY_PART = re.compile(r'\{\s*(MODEL_\w+)\s*,\s*L"(\w+)"\s*\}')
BODY_FOLDER = re.compile(r'OriginalPlayerModelFolder\s*=\s*L"((?:[^"\\]|\\.)*)"')
BODY_TEXTURE_FOLDER = re.compile(r'const\s+std::wstring\s+textureSubfolder\s*=\s*L"((?:[^"\\]|\\.)*)"')
BODY_LOADER = 'OpenPlayerBodyParts'
# The lines body_models() follows; a changed one stops the run instead of leaving a stale rule.
BODY_RULES = (
    (BODY_FILE, 'const int classOffset = stage * MAX_CLASS + baseClass;'),
    (BODY_FILE, 'OpenBodyPart(part.firstSlot + classOffset, PlayerBodyFileName(stage, part, baseClass), useS6PlayerModels);'),
    (BODY_FILE, 'if (stage > 0) name += std::to_wstring(stage + 1);'),
    (BODY_FILE, "const int number = baseClass + 1; if (number < 10) name += L'0'; return name + std::to_wstring(number);"),
    (BODY_FILE, 'return stage != 1 || (baseClass != CLASS_DARK && baseClass != CLASS_DARK_LORD && '
                'baseClass != CLASS_RAGEFIGHTER);'),
    (BODY_FILE, 'for (int stage = 0; stage < MAX_CLASS_STAGES; ++stage) { if (HasPlayerBodyForm(stage, baseClass)) '
                'OpenPlayerBodyParts(stage, baseClass, useS6PlayerModels); }'),
    (BODY_FILE, 'gLoadData.AccessModel(slot, Engine::Object::OriginalPlayerModelFolder, fileName.c_str());'),
    (OPEN_DATA, 'for (int i = 0; i < MAX_CLASS; ++i) { Engine::Object::OpenPlayerClassBodyParts(i, useS6); }'),
    (OPEN_DATA, 'if (Engine::Object::HasPlayerBodyForm(j, i)) Engine::Object::OpenPlayerBodyTextures(j, i);'),
)
SECOND_STAGE = 1
SINGLE_DIGITS = 10

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

    def resolve_function(self, name):
        """A plain function of the load file that a load function calls (OpenItemEffectModels)."""
        body, line = function_body(self.text(OPEN_DATA), name)
        return (name, body, line) if body is not None else None

    # --- running the load code -----------------------------------------------------------

    def run_loads(self):
        runner = Runner(self.symbols, (ACCESS_MODEL, OPEN_TEXTURE), self.resolve_method, self.resolve_function)
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
                # One folder, or the folders of a std::wstring array, in the order they are searched.
                folders = call.args[1] if isinstance(call.args[1], list) else [call.args[1]]
                for name in folders:
                    folder = DATA_PREFIX + name.replace('\\', '/')
                    if folder not in model['texture_dirs']:
                        model['texture_dirs'].append(folder)
        return models

    def add_item_data_models(self, models):
        """Adds the item models of Data/Items/Models (see the module text) to `models`."""
        folder = self.data_parent / ITEM_MODEL_DATA
        shared_document = json.loads((folder / SHARED_MODEL_FILE).read_text(encoding='utf-8'))
        shared = {entry['name']: entry for entry in shared_document['models']}
        for path in sorted(folder.glob(ITEM_MODEL_FILES)):
            document = json.loads(path.read_text(encoding='utf-8'))
            source = f'{ITEM_MODEL_DATA}/{path.name}'
            for entry in document['models']:
                key = f'{document["group"]}-{entry["number"]}'
                loaded_by, files = source, entry
                if 'model' in entry:
                    if entry['model'] not in shared:
                        raise Unresolved(f'{source}: item {key} names the shared model {entry["model"]!r}, '
                                         f'which {SHARED_MODEL_FILE} does not have')
                    loaded_by, files = f'{source} -> {SHARED_MODEL_FILE} ({entry["model"]})', shared[entry['model']]
                if 'file' not in files:
                    continue
                model_type = self.model_item + document['group'] * self.max_index + entry['number']
                load = {'referenced': files['file'].replace('\\', '/'), 'loaded_by': loaded_by}
                texture_dirs = [DATA_PREFIX + folder_name.replace('\\', '/').strip('/')
                                for folder_name in files.get('textureFolders', [])]
                models[model_type] = {'load': load, 'earlier': [], 'texture_dirs': texture_dirs}

    def body_models(self, models):
        """Adds the unequipped body parts of every class form (see the module text) to `models`."""
        for file, line in BODY_RULES:
            if line not in ' '.join(self.text(file).split()):
                raise Unresolved(f'{file}: the body part rule {line!r} is not there any more')
        header, code = self.text(BODY_HEADER), self.text(BODY_FILE)
        parts, folder, textures = BODY_PARTS.search(header), BODY_FOLDER.search(header), BODY_TEXTURE_FOLDER.search(code)
        if not (parts and folder and textures):
            raise Unresolved(f'{BODY_HEADER}: the body part table, its folder or its texture folder was not found')
        value = self.symbols.value
        classes, without_second = value('MAX_CLASS'), {value(name) for name in ('CLASS_DARK', 'CLASS_DARK_LORD',
                                                                                'CLASS_RAGEFIGHTER')}
        directory = folder.group(1).replace('\\\\', '/')
        texture_dir = DATA_PREFIX + textures.group(1).replace('\\\\', '/').strip('/')
        for base_class in range(classes):
            for stage in range(value('MAX_CLASS_STAGES')):
                if stage == SECOND_STAGE and base_class in without_second:
                    continue
                for slot, prefix in BODY_PART.findall(parts.group(1)):
                    number = base_class + 1
                    name = prefix + (str(stage + 1) if stage > 0 else '') + ('0' if number < SINGLE_DIGITS else '') + str(number)
                    load = {'referenced': f'{directory}{name}.bmd', 'loaded_by': BODY_LOADER}
                    models[value(slot) + stage * classes + base_class] = {'load': load, 'earlier': [],
                                                                         'texture_dirs': [texture_dir]}

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
        """Items the inventory draws with another model than their own (ItemDisplay.cpp): the Rage
        Fighter armour's inventory models, and FindInventoryModel's level variants. One link per model
        and run of levels; the condition is written like the C++ one."""
        text = self.text(INVENTORY_FILE)
        armour = self.armour_inventory_models(text)
        variants = self.inventory_variants(text)
        links = []
        for item_type in sorted(self.item_types):
            own = self.model_item + item_type
            armour_item, inventory_model = armour.get(item_type, (None, own))
            type_condition, by_level = (f'Type == {armour_item}' if armour_item else None), {}
            for level in ITEM_LEVELS:
                model, condition = variants(item_type, level, inventory_model)
                by_level[level] = model
                type_condition = condition or type_condition
            for model in sorted(set(by_level.values()) - {own, INVENTORY_NO_MODEL}):
                levels = [level for level in ITEM_LEVELS if by_level[level] == model]
                for first, last in level_runs(levels):
                    whole = (first, last) == (ITEM_LEVELS[0], ITEM_LEVELS[-1])
                    if type_condition is None:
                        raise Unresolved(f'{INVENTORY_FILE}: no condition on the item type for item {item_type}')
                    condition = type_condition if whole else f'{type_condition} && {level_condition(first, last)}'
                    links.append((item_type, model, 'inventory', condition))
        return links

    def armour_inventory_models(self, text):
        """ArmorInventoryModels: item type -> (item name, inventory model type)."""
        table = ARMOR_INVENTORY_TABLE.search(text)
        if table is None:
            raise Unresolved(f'{INVENTORY_FILE}: the ArmorInventoryModels table was not found')
        value = lambda name: evaluate(tokenize(name), self.symbols.value)  # noqa: E731
        return {value(item): (item, value(model)) for item, model in ARMOR_INVENTORY_ENTRY.findall(table.group(1))}

    def inventory_variants(self, text):
        """FindInventoryModel as a function (item type, level, inventory model) -> (model type or
        INVENTORY_NO_MODEL, the C++ condition on the item type that chose it). The function is a
        switch over the item type whose cases are `if (...) return ...;`, `return ...;` and `break;`;
        anything else raises instead of being guessed."""
        body, _ = function_body(text, INVENTORY_FUNCTION)
        if body is None or INVENTORY_SWITCH not in body:
            raise Unresolved(f'{INVENTORY_FILE}: {INVENTORY_FUNCTION} with {INVENTORY_SWITCH!r} was not found')
        if sorted(INVENTORY_LAMBDA.findall(body)) != sorted(INVENTORY_LAMBDAS):
            raise Unresolved(f'{INVENTORY_FILE}: {INVENTORY_FUNCTION} no longer has the lambdas {INVENTORY_LAMBDAS}')
        groups, end = render_code.parse_switch(body, body.index(INVENTORY_SWITCH), 1)
        if ' '.join(body[end:].split()) != INVENTORY_FALLBACK:
            raise Unresolved(f'{INVENTORY_FILE}: {INVENTORY_FUNCTION} does not end with {INVENTORY_FALLBACK!r}')
        cases, default = {}, []
        for labels, _, case_body, _ in groups:
            statements = inventory_statements(case_body)
            for label in labels:
                if label == 'default':
                    default = statements
                else:
                    cases[evaluate(tokenize(label), self.symbols.value)] = (label, statements)

        def variant(item_type, level, inventory_model):
            names = {'itemType': item_type, 'level': level, 'inventoryModel': inventory_model,
                     INVENTORY_NOTHING: INVENTORY_NO_MODEL}
            lookup = lambda name: names[name] if name in names else self.symbols.value(name)  # noqa: E731
            label, statements = cases.get(item_type, (None, default))
            for condition, result in statements:
                if condition is not None and not evaluate(tokenize(condition), lookup):
                    continue
                if result is None:
                    break
                chosen = None
                if label is not None:
                    chosen = f'Type == {label}'
                elif condition is not None and ITEM_TYPE_WORD.search(condition):
                    chosen = ITEM_TYPE_WORD.sub('Type', ' '.join(condition.split()))
                return evaluate(tokenize(result), lookup), chosen
            return inventory_model, None
        return variant

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
        self.body_models(models)
        self.add_item_data_models(models)
        items, other = {}, {}
        for model_type, model in models.items():
            if model['load'] is None:
                continue
            if self.is_item(model_type):
                key, group, index = self.item_key(model_type)
                items[key] = dict(self.model_record(model), group=group, index=index, extra_models=[])
            else:
                other[model_type] = model
        self.item_types = {item['group'] * self.max_index + item['index'] for item in items.values()}
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
            'sources': sorted({f'src/source/{p}' for p in list(self.sources) + list(GLOBAL_HEADERS)}
                              | {f'src/bin/{ITEM_MODEL_DATA}'}),
            'macros': dict(sorted(self.consulted.items())),
            'counts': {'items': len(items), 'extra_models': sum(len(i['extra_models']) for i in items.values()),
                       'other_models': len(other_models), 'missing_files': len(missing)},
            'items': items,
            'socket_items': ['{}-{}'.format(*divmod(t, self.max_index)) for t in self.socket_items()],
            'other_models': other_models,
            'missing_files': [{'item': key, 'path': path} for key, path in missing],
            'skipped_statements': runner.skipped,
        }


def inventory_statements(case_body):
    """The statements of one case of FindInventoryModel as (condition or None, result expression or
    None for `break`), in order."""
    statements = []
    for raw in split_statements(case_body):
        text, condition = raw.strip(), None
        if not text:
            continue
        if INVENTORY_IF.match(text):
            open_paren = text.index('(')
            close_paren = render_code.matching(text, open_paren, '(', ')')
            condition, text = text[open_paren + 1:close_paren - 1], text[close_paren:].strip()
        returned = INVENTORY_RETURN.match(text)
        if returned:
            statements.append((condition, inventory_expression(returned.group(1))))
        elif text == 'break' and condition is None:
            statements.append((None, None))
        else:
            raise Unresolved(f'{INVENTORY_FILE}: {INVENTORY_FUNCTION} has a statement this tool does not read: '
                             f'{raw.strip()!r}')
    return statements


def split_statements(text):
    """`text` cut at every ';' outside brackets."""
    parts, depth, current = [], 0, []
    for char in text:
        if char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
        if char == ';' and depth == 0:
            parts.append(''.join(current))
            current = []
        else:
            current.append(char)
    parts.append(''.join(current))
    return parts


def inventory_expression(text):
    for old, new in INVENTORY_REWRITES:
        text = text.replace(old, new)
    for name in INVENTORY_LAMBDAS:
        text = re.sub(rf'\b{name}\s*\(', '(', text)
    return text


def level_runs(levels):
    """Sorted levels as (first, last) runs of consecutive levels."""
    runs = []
    for level in levels:
        if runs and runs[-1][1] == level - 1:
            runs[-1] = (runs[-1][0], level)
        else:
            runs.append((level, level))
    return runs


def level_condition(first, last):
    return f'Level == {first}' if first == last else f'{first} <= Level && Level <= {last}'


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
