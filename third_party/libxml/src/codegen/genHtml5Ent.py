#!/usr/bin/env python3

import json
import sys
from dataclasses import dataclass

# The basic idea is to find named character references using binary
# search. Since entity strings may not have a terminator, this doesn't
# work if one entity string is a prefix of another. In this case,
# we branch to a subtable after matching the prefix.
#
# We create separate initial tables based on the first character
# of the entity name.
#
# The following tables are generated:
#
# htmlEntAlpha:   start and end of initial tables, indexing into
#                 htmlEntValues
# htmlEntValues:  concatenation of all table values, which index into
#                 htmlEntStrings
# htmlEntStrings: variable sized records containing entity name,
#                 replacement and optionally the position of a
#                 subtable

try:
    with open('entities.json') as json_data:
        ents = json.load(json_data)
except FileNotFoundError:
    print('entities.json not found, try curl -LJO',
          'https://html.spec.whatwg.org/entities.json')
    sys.exit(1)

def to_cchars(s):
    r = []

    for c in s.encode():
        if c >= 0x20 and c <= 0x7E and c != ord("'") and c != ord('\\'):
            v = f"'{chr(c)}'"
        else:
            v = c
        r += [ v ]

    return r

@dataclass
class PrefixStackEntry:
    prefix: str
    table_id: int

@dataclass
class AlphaFixup:
    table_id: int
    char: int

@dataclass
class StringFixup:
    table_id: int
    string_index: int
    super_table_id: int
    super_offset: int

# Remove entity strings without trailing semicolon
keys = (key for key in ents.keys() if key.endswith(';'))

# Sort entity strings
keys = sorted(keys, key=lambda k: k[1:-1])

strings = []
tables = []
prefix_stack = []
alpha_fixups = []
string_fixups = []
for i in range(64):
    tables.append([])

for i, key in enumerate(keys):
    name = key[1:-1]

    next_name = None
    if i + 1 < len(keys):
        next_name = keys[i+1][1:-1]

    while prefix_stack and not name.startswith(prefix_stack[-1].prefix):
        prefix_stack.pop()

    # First character is initial prefix
    if not prefix_stack:
        table_id = len(tables)
        tables.append([])

        prefix_stack.append(PrefixStackEntry(name[0], table_id))
        alpha_fixups.append(AlphaFixup(table_id, ord(name[0]) % 64))

    string_index = len(strings)
    table = tables[prefix_stack[-1].table_id]
    table_index = len(table)
    table.append(string_index)

    name_offset = len(prefix_stack[-1].prefix)
    name_chars = to_cchars(name[name_offset:])
    repl_chars = to_cchars(ents[key]['characters'])
    semicolon_flag = 0
    if key[:-1] in ents:
        semicolon_flag = 0x80

    if next_name and next_name.startswith(name):
        # Create subtable

        strings += [
            len(name_chars) | semicolon_flag | 0x40, *name_chars,
            0, 0, # subtable position, to be fixed up
            len(repl_chars), *repl_chars,
        ]

        table_id = len(tables)
        tables.append([])

        fixup_index = string_index + 1 + len(name_chars)
        string_fixups.append(StringFixup(
            table_id, fixup_index, prefix_stack[-1].table_id, table_index,
        ))

        prefix_stack.append(PrefixStackEntry(name, table_id))
    else:
        strings += [
            len(name_chars) | semicolon_flag, *name_chars,
            len(repl_chars), *repl_chars,
        ]

# Concat tables and record ranges
ranges = [ 0 ]
values = []
for table in tables:
    values += table
    ranges.append(len(values))

# Create alpha table
alpha = [ 0 ] * (59 * 3)
for fixup in alpha_fixups:
    table_id, c = fixup.table_id, fixup.char
    start = ranges[table_id]
    end = ranges[table_id+1]
    alpha[c*3:c*3+3] = [ start & 0xFF, start >> 8, end - start ]

# Fix up subtable positions
for fixup in string_fixups:
    table_id, i = fixup.table_id, fixup.string_index
    start = ranges[table_id]
    end = ranges[table_id+1]
    super_index = ranges[fixup.super_table_id] + fixup.super_offset
    strings[i:i+2] = [ start - super_index, end - start ]

# Print tables

def gen_table(ctype, cname, values, fmt, elems_per_line):
    count = len(values)
    r = ''

    for i in range(count):
        if i != 0: r += ','
        if i % elems_per_line == 0: r += '\n    '
        else: r += ' '
        r += fmt % values[i]

    return f'static const {ctype} {cname}[{count}] = {{{r}\n}};\n\n'

with open('codegen/html5ent.inc', 'w') as out:
    out.write(gen_table('unsigned char', 'htmlEntAlpha', alpha, '%3d', 15))
    out.write(gen_table('unsigned short', 'htmlEntValues', values, '%5d', 10))
    out.write(gen_table('unsigned char', 'htmlEntStrings', strings, '%3s', 15))
