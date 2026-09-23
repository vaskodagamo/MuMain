#!/usr/bin/env python3
"""Export OpenMU's item definitions into assets-work/Items/openmu-items.json; standard library only.

The server knows facts the client table does not (OpenMU DataModel ItemDefinition): DropLevel,
MaximumDropLevel, DropsFromMonsters, MaximumItemLevel, MaximumSockets, IsAmmunition, the qualified
character classes, PossibleItemSetGroups and PossibleItemOptions. build_item_catalog.py folds the
export into the catalog when the file exists.

Sources (pick one):
    --backup <file>      the admin panel backup (GET admin/backup, a zip with GameConfiguration_*.json)
                         or one extracted GameConfiguration_*.json. Objects are linked with
                         System.Text.Json "$id" / "$ref"; scalar properties are PascalCase and the
                         collections camelCase (Persistence/BasicModel/*.Generated.cs).
    --postgres <name>    read-only SELECT through `docker exec <name> psql -U postgres -w` (the local
                         OpenMU database container, "database" in the default compose file). -w never
                         prompts: when the container needs a password the export fails instead.

Usage:
    python3 tools/item_editor/export_openmu_items.py --postgres database
    python3 tools/item_editor/export_openmu_items.py --backup ~/Downloads/openmu-backup.zip
The output has sorted keys and no timestamps, so an unchanged server gives an unchanged file.
"""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUTPUT = ROOT / 'assets-work' / 'Items' / 'openmu-items.json'
SCHEMA = 'mu-openmu-items/1'
GAME_CONFIGURATION_PREFIX = 'GameConfiguration_'
ID_KEY, REF_KEY = '$id', '$ref'
DATABASE = 'openmu'
POSTGRES_USER = 'postgres'
EXCELLENT_OPTION_WORD = 'Excellent'

# One row per item definition, in the same shape the backup reader produces.
ITEMS_SQL = '''
select coalesce(json_agg(row_to_json(t) order by t."Group", t."Number"), '[]') from (
  select i."Group", i."Number", i."Name", i."DropLevel", i."MaximumDropLevel", i."DropsFromMonsters",
         i."MaximumItemLevel", i."MaximumSockets", i."Width", i."Height", i."IsAmmunition",
         coalesce((select json_agg(c."Name" order by c."Number")
                   from config."ItemDefinitionCharacterClass" l
                   join config."CharacterClass" c on c."Id" = l."CharacterClassId"
                   where l."ItemDefinitionId" = i."Id"), '[]') as "QualifiedCharacters",
         coalesce((select json_agg(json_build_object('Name', s."Name", 'AlwaysApplies', s."AlwaysApplies") order by s."Name")
                   from config."ItemDefinitionItemSetGroup" l
                   join config."ItemSetGroup" s on s."Id" = l."ItemSetGroupId"
                   where l."ItemDefinitionId" = i."Id"), '[]') as "PossibleItemSetGroups",
         coalesce((select json_agg(o."Name" order by o."Name")
                   from config."ItemDefinitionItemOptionDefinition" l
                   join config."ItemOptionDefinition" o on o."Id" = l."ItemOptionDefinitionId"
                   where l."ItemDefinitionId" = i."Id"), '[]') as "PossibleItemOptions"
  from config."ItemDefinition" i) t;
'''

SCALAR_FIELDS = ('Group', 'Number', 'Name', 'DropLevel', 'MaximumDropLevel', 'DropsFromMonsters',
                 'MaximumItemLevel', 'MaximumSockets', 'Width', 'Height', 'IsAmmunition')


class ExportError(RuntimeError):
    pass


# --- backup ---------------------------------------------------------------------------------

def read_backup_documents(path):
    """The GameConfiguration JSON document(s) of a backup zip or a single extracted file."""
    path = Path(path)
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            names = sorted(n for n in archive.namelist() if Path(n).name.startswith(GAME_CONFIGURATION_PREFIX))
            if not names:
                raise ExportError(f'{path}: no {GAME_CONFIGURATION_PREFIX}*.json in the backup')
            return [json.loads(archive.read(name).decode('utf-8-sig')) for name in names]
    return [json.loads(path.read_text(encoding='utf-8-sig'))]


def index_ids(node, found):
    """Every object with a "$id", so "$ref" links can be followed."""
    if isinstance(node, dict):
        if ID_KEY in node:
            found[str(node[ID_KEY])] = node
        for value in node.values():
            index_ids(value, found)
    elif isinstance(node, list):
        for value in node:
            index_ids(value, found)
    return found


def field(node, name):
    """A property by PascalCase or camelCase name."""
    for candidate in (name, name[0].lower() + name[1:]):
        if candidate in node:
            return node[candidate]
    return None


def values_list(node):
    """A serialized collection: a list, or {"$values": [...]} (reference-preserving arrays)."""
    if isinstance(node, dict) and '$values' in node:
        return node['$values']
    return node if isinstance(node, list) else []


def resolve(node, ids):
    if isinstance(node, dict) and REF_KEY in node:
        return ids.get(str(node[REF_KEY]), {})
    return node if isinstance(node, dict) else {}


def rows_from_backup(documents):
    rows = []
    for document in documents:
        ids = index_ids(document, {})
        for item in values_list(field(document, 'Items')):
            item = resolve(item, ids)
            row = {name: field(item, name) for name in SCALAR_FIELDS}
            row['QualifiedCharacters'] = [field(resolve(c, ids), 'Name') for c in values_list(field(item, 'QualifiedCharacters'))]
            row['PossibleItemSetGroups'] = [{'Name': field(resolve(s, ids), 'Name'), 'AlwaysApplies': field(resolve(s, ids), 'AlwaysApplies')}
                                            for s in values_list(field(item, 'PossibleItemSetGroups'))]
            row['PossibleItemOptions'] = [field(resolve(o, ids), 'Name') for o in values_list(field(item, 'PossibleItemOptions'))]
            rows.append(row)
    return rows


# --- postgres -------------------------------------------------------------------------------

def rows_from_postgres(container):
    command = ['docker', 'exec', container, 'psql', '-U', POSTGRES_USER, '-w', '-d', DATABASE, '-At', '-c', ITEMS_SQL]
    try:
        done = subprocess.run(command, capture_output=True, text=True, check=False)
    except OSError as error:
        raise ExportError(f'docker is not available: {error}') from error
    if done.returncode != 0:
        raise ExportError(f'psql in {container} failed: {done.stderr.strip()}')
    return json.loads(done.stdout)


# --- normalised output ----------------------------------------------------------------------

def normalise(rows):
    """Rows (either source) -> {key: item facts}, sorted and free of source details."""
    items = {}
    for row in rows:
        group, number = row['Group'], row['Number']
        if group is None or number is None:
            continue
        set_groups = sorted({(s['Name'], bool(s['AlwaysApplies'])) for s in row['PossibleItemSetGroups'] if s.get('Name')})
        options = sorted({name for name in row['PossibleItemOptions'] if name})
        items[f'{group}-{number}'] = {
            'group': group,
            'number': number,
            'name': row['Name'],
            'drop_level': row['DropLevel'],
            'maximum_drop_level': row['MaximumDropLevel'],
            'drops_from_monsters': bool(row['DropsFromMonsters']),
            'maximum_item_level': row['MaximumItemLevel'],
            'maximum_sockets': row['MaximumSockets'] or 0,
            'width': row['Width'],
            'height': row['Height'],
            'is_ammunition': bool(row['IsAmmunition']),
            'qualified_classes': sorted(name for name in row['QualifiedCharacters'] if name),
            'ancient_sets': [name for name, always in set_groups if not always],
            'set_bonus_groups': [name for name, always in set_groups if always],
            'options': options,
            'excellent': any(EXCELLENT_OPTION_WORD in name for name in options),
        }
    return items


def document(items, source):
    return {'schema': SCHEMA, 'source': source, 'count': len(items), 'items': items}


def render(doc):
    return json.dumps(doc, indent=1, sort_keys=True, ensure_ascii=False) + '\n'


def main(argv=None):
    parser = argparse.ArgumentParser(description='Export OpenMU item definitions for the item catalog.')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--backup', type=Path, help='admin backup zip or GameConfiguration_*.json')
    source.add_argument('--postgres', metavar='CONTAINER', help='docker container of the OpenMU database')
    parser.add_argument('--output', type=Path, default=OUTPUT)
    args = parser.parse_args(argv)
    try:
        if args.backup:
            rows, origin = rows_from_backup(read_backup_documents(args.backup)), {'kind': 'backup', 'file': args.backup.name}
        else:
            rows, origin = rows_from_postgres(args.postgres), {'kind': 'postgres', 'database': DATABASE}
    except (OSError, ValueError, ExportError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    items = normalise(rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(document(items, origin)), encoding='utf-8')
    print(f'wrote {args.output}: {len(items)} item definitions')
    return 0


if __name__ == '__main__':
    sys.exit(main())
