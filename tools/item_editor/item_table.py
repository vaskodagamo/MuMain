#!/usr/bin/env python3
"""Read the client's item tables; standard library only (Python 3.9+).

Files (all 16 groups x 512 indexes = 8192 records, the index of a record is its item type):

    Data/Local/<Lang>/Item_<lang>.bmd   ITEM_ATTRIBUTE records (ItemDataLoader.cpp)
    Data/Local/ItemSetType.bmd          which ancient sets an item can belong to (CSItemOption.cpp)
    Data/Local/ItemAddOption.bmd        the "380" add option of an item (ItemAddOptioninfo.cpp)

Item_<lang>.bmd comes in two layouts that differ only in the length of the name field, and the
loader tells them apart by file size: the legacy (Season 6 Episode 3) layout has 30-byte names,
the current one MAX_ITEM_NAME = 50-byte names. Each record is XOR-ed on its own with the
BuxConvert key {0xFC, 0xCF, 0xAB} (the key index restarts at every record), and the file ends in a
32-bit checksum of the still-encrypted records (GenerateCheckSum2, key 0xE2F1). The record layout is
the ITEM_FIELDS_SIMPLE / ITEM_ATTRIBUTE_FIELDS X-macro of ItemFieldDefs.h with natural C alignment
(no #pragma pack).

Usage:
    python3 tools/item_editor/item_table.py src/bin/Data/Local/Eng/item_eng.bmd        # summary
    python3 tools/item_editor/item_table.py src/bin/Data/Local/Eng/item_eng.bmd --json # every named item
"""

import argparse
import json
from pathlib import Path
import struct
import sys

MAX_ITEM_TYPE = 16
MAX_ITEM_INDEX = 512
MAX_ITEM = MAX_ITEM_TYPE * MAX_ITEM_INDEX
MAX_CLASS = 7
RESISTANCE_COUNT = 8  # MAX_RESISTANCE + 1

BUX_CODE = (0xFC, 0xCF, 0xAB)
ITEM_CHECKSUM_KEY = 0xE2F1
SET_TYPE_CHECKSUM_KEY = 0xE5F1
CHECKSUM_SIZE = 4
DWORD_MASK = 0xFFFFFFFF
CHECKSUM_KEY_SHIFT = 9
CHECKSUM_MIX_EVERY = 16

LEGACY_NAME_BYTES = 30
CURRENT_NAME_BYTES = 50
FORMAT_LEGACY = 'legacy'
FORMAT_CURRENT = 'current'

# RequireClass[7] order (ItemFieldDefs.h): value = minimum class stage, 0 = not usable.
CLASS_KEYS = ('dw', 'dk', 'elf', 'mg', 'dl', 'sum', 'rf')

# ITEM_FIELDS_SIMPLE in declaration order: (field, struct code). The test compares this list with
# ItemFieldDefs.h, so a change to the C++ record shows up as a failing test instead of garbage.
SIMPLE_FIELDS = (
    ('TwoHand', '?'),
    ('Level', 'H'),
    ('m_byItemSlot', 'B'),
    ('m_wSkillIndex', 'H'),
    ('Width', 'B'),
    ('Height', 'B'),
    ('DamageMin', 'B'),
    ('DamageMax', 'B'),
    ('SuccessfulBlocking', 'B'),
    ('Defense', 'B'),
    ('MagicDefense', 'B'),
    ('WeaponSpeed', 'B'),
    ('WalkSpeed', 'B'),
    ('Durability', 'B'),
    ('MagicDur', 'B'),
    ('MagicPower', 'B'),
    ('RequireStrength', 'H'),
    ('RequireDexterity', 'H'),
    ('RequireEnergy', 'H'),
    ('RequireVitality', 'H'),
    ('RequireCharisma', 'H'),
    ('RequireLevel', 'H'),
    ('Value', 'B'),
    ('iZen', 'i'),
    ('AttType', 'B'),
)

# ITEM_SET_TYPE: byOption[2], byMixItemLevel[2]. CSItemOption.cpp compares byOption with
# EMPTY_OPTION 0xFF, but the shipped ItemSetType.bmd stores 0 in both slots for "no set" (8024 of
# 8192 records; set number 0 does occur, but only next to a second set). A record whose two set
# numbers are both 0 therefore counts as "no set" here.
SET_TYPE_RECORD = struct.Struct('<2B2B')
NO_SET = (0, 0)
# ITEM_ADD_OPTION: BYTE option1, WORD value1, BYTE option2, WORD value2, BYTE type, DWORD time.
ADD_OPTION_RECORD = struct.Struct('<BxHBxHB3xI')


class ItemTableError(ValueError):
    """The file is not a valid item table (size, checksum or layout)."""


def item_key(group, index):
    return f'{group}-{index}'


def split_type(item_type):
    return divmod(item_type, MAX_ITEM_INDEX)


def record_struct(name_bytes):
    """struct.Struct of one Item_<lang>.bmd record with the given name length.

    Every field is placed at its natural alignment ('@' would use the host's rules; the explicit
    padding below keeps the layout identical on every platform).
    """
    fmt = ['<', f'{name_bytes}s']
    offset = name_bytes
    for _, code in SIMPLE_FIELDS:
        size = struct.calcsize('<' + code)
        padding = -offset % size
        if padding:
            fmt.append(f'{padding}x')
        fmt.append(code)
        offset += padding + size
    fmt.append(f'{MAX_CLASS}B{RESISTANCE_COUNT}B')
    offset += MAX_CLASS + RESISTANCE_COUNT
    widest = struct.calcsize('<i')
    tail = -offset % widest
    if tail:
        fmt.append(f'{tail}x')
    return struct.Struct(''.join(fmt))


RECORDS = {FORMAT_LEGACY: record_struct(LEGACY_NAME_BYTES), FORMAT_CURRENT: record_struct(CURRENT_NAME_BYTES)}


def bux_convert(data, start=0):
    """XOR with the BuxConvert key; symmetric. start is the key index of data[0]."""
    return bytes(value ^ BUX_CODE[(start + i) % len(BUX_CODE)] for i, value in enumerate(data))


def checksum2(data, key):
    """GenerateCheckSum2 (ZzzInfomation.h) over data, whose length is a multiple of four."""
    result = (key << CHECKSUM_KEY_SHIFT) & DWORD_MASK
    words = struct.unpack(f'<{len(data) // 4}I', data[:len(data) // 4 * 4])
    for number, word in enumerate(words):
        offset = number * 4
        if (number + key) % 2 == 0:
            result ^= word
        else:
            result = (result + word) & DWORD_MASK
        if offset % CHECKSUM_MIX_EVERY == 0:
            result ^= ((key + result) & DWORD_MASK) >> (number % 8 + 1)
    return result


def detect_format(size):
    """Layout name for a file of this size, the same test ItemDataLoader::Load makes."""
    for name, record in RECORDS.items():
        if size == record.size * MAX_ITEM + CHECKSUM_SIZE:
            return name
    expected = ', '.join(f'{r.size * MAX_ITEM + CHECKSUM_SIZE} ({n})' for n, r in RECORDS.items())
    raise ItemTableError(f'{size} bytes is neither item table layout ({expected})')


def decode_name(raw):
    return raw.split(b'\0', 1)[0].decode('utf-8', errors='replace')


def decode_record(raw, record):
    values = record.unpack(raw)
    name, simple = values[0], values[1:1 + len(SIMPLE_FIELDS)]
    rest = values[1 + len(SIMPLE_FIELDS):]
    fields = {field: value for (field, _), value in zip(SIMPLE_FIELDS, simple)}
    fields['Name'] = decode_name(name)
    fields['RequireClass'] = list(rest[:MAX_CLASS])
    fields['Resistance'] = list(rest[MAX_CLASS:])
    return fields


def read_item_table(data):
    """Decode Item_<lang>.bmd bytes -> {'format', 'checksum', 'items': [8192 field dicts]}."""
    layout = detect_format(len(data))
    record = RECORDS[layout]
    body, stored = data[:-CHECKSUM_SIZE], struct.unpack('<I', data[-CHECKSUM_SIZE:])[0]
    computed = checksum2(body, ITEM_CHECKSUM_KEY)
    if computed != stored:
        raise ItemTableError(f'checksum mismatch: file says {stored:#010x}, records give {computed:#010x}')
    items = []
    for item_type in range(MAX_ITEM):
        raw = bux_convert(body[item_type * record.size:(item_type + 1) * record.size])
        items.append(decode_record(raw, record))
    return {'format': layout, 'checksum': stored, 'items': items}


def encode_item_table(items, layout=FORMAT_LEGACY):
    """Inverse of read_item_table (used by the tests and by tools that write the table)."""
    record = RECORDS[layout]
    name_bytes = LEGACY_NAME_BYTES if layout == FORMAT_LEGACY else CURRENT_NAME_BYTES
    chunks = []
    for fields in items:
        name = fields['Name'].encode('utf-8')[:name_bytes - 1]
        values = [name] + [fields[f] for f, _ in SIMPLE_FIELDS] + list(fields['RequireClass']) + list(fields['Resistance'])
        chunks.append(bux_convert(record.pack(*values)))
    body = b''.join(chunks)
    return body + struct.pack('<I', checksum2(body, ITEM_CHECKSUM_KEY))


def empty_item():
    fields = {field: (False if code == '?' else 0) for field, code in SIMPLE_FIELDS}
    fields.update(Name='', RequireClass=[0] * MAX_CLASS, Resistance=[0] * RESISTANCE_COUNT)
    return fields


def read_set_types(data):
    """ItemSetType.bmd -> {item_type: [set number, set number]} for items that can be ancient."""
    body_size = SET_TYPE_RECORD.size * MAX_ITEM
    if len(data) != body_size + CHECKSUM_SIZE:
        raise ItemTableError(f'ItemSetType.bmd: expected {body_size + CHECKSUM_SIZE} bytes, got {len(data)}')
    body, stored = data[:body_size], struct.unpack('<I', data[body_size:])[0]
    if checksum2(body, SET_TYPE_CHECKSUM_KEY) != stored:
        raise ItemTableError('ItemSetType.bmd: checksum mismatch')
    sets = {}
    for item_type in range(MAX_ITEM):
        chunk = body[item_type * SET_TYPE_RECORD.size:(item_type + 1) * SET_TYPE_RECORD.size]
        options = SET_TYPE_RECORD.unpack(bux_convert(chunk))[:2]
        if options != NO_SET:
            sets[item_type] = list(options)
    return sets


def read_add_options(data):
    """ItemAddOption.bmd -> {item_type: {option1, value1, option2, value2, type, time}} for items with one.

    Unlike the item table, the whole buffer is XOR-ed in one pass (the key index runs across
    records) and there is no checksum.
    """
    expected = ADD_OPTION_RECORD.size * MAX_ITEM
    if len(data) < expected:
        raise ItemTableError(f'ItemAddOption.bmd: expected {expected} bytes, got {len(data)}')
    plain = bux_convert(data[:expected])
    options = {}
    for item_type in range(MAX_ITEM):
        option1, value1, option2, value2, kind, time = ADD_OPTION_RECORD.unpack_from(plain, item_type * ADD_OPTION_RECORD.size)
        if option1 or option2:
            options[item_type] = {'option1': option1, 'value1': value1, 'option2': option2, 'value2': value2,
                                  'type': kind, 'time': time}
    return options


def named_items(table):
    """(item_type, fields) for every record with a name."""
    return [(item_type, fields) for item_type, fields in enumerate(table['items']) if fields['Name']]


def summary(path):
    table = read_item_table(Path(path).read_bytes())
    named = named_items(table)
    return {'file': str(path), 'format': table['format'], 'checksum': f'{table["checksum"]:#010x}',
            'named_items': len(named)}


def main(argv=None):
    parser = argparse.ArgumentParser(description='Decode a client item table (Item_<lang>.bmd).')
    parser.add_argument('path', help='Item_<lang>.bmd')
    parser.add_argument('--json', action='store_true', help='print every named item as JSON')
    args = parser.parse_args(argv)
    try:
        if not args.json:
            print(json.dumps(summary(args.path), indent=1))
            return 0
        table = read_item_table(Path(args.path).read_bytes())
        rows = {item_key(*split_type(t)): fields for t, fields in named_items(table)}
        print(json.dumps(rows, indent=1, ensure_ascii=False))
    except (OSError, ItemTableError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
