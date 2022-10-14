#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dumps the names, addresses, and disassmebly of static initializers.

Usage example:
  tools/linux/dump-static-intializers.py out/Release/chrome

For an explanation of static initializers, see: //docs/static_initializers.md.
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys

_TOOLCHAIN_PREFIX = str(
    pathlib.Path(__file__).parents[2] / 'third_party' / 'llvm-build' /
    'Release+Asserts' / 'bin' / 'llvm-')

# It is too slow to dump disassembly for a lot of symbols.
_MAX_DISASSEMBLY_SYMBOLS = 10


def _ParseNm(binary, addresses):
  # Example output:
  # 000000000de66bd0 0000000000000026 t _GLOBAL__sub_I_add.cc
  output = subprocess.check_output(
      [_TOOLCHAIN_PREFIX + 'nm', '--print-size', binary], encoding='utf8')
  addresses = set(addresses)
  ret = {}
  for line in output.splitlines():
    parts = line.split()
    if len(parts) != 4:
      continue
    address = int(parts[0], 16)
    if address in addresses:
      ret[address] = int(parts[1], 16)
  return ret


def _Disassemble(binary, start, end):
  cmd = [
      _TOOLCHAIN_PREFIX + 'objdump',
      binary,
      '--disassemble',
      '--source',
      '--demangle',
      '--start-address=0x%x' % start,
      '--stop-address=0x%x' % end,
  ]
  stdout = subprocess.check_output(cmd, encoding='utf8')
  all_lines = stdout.splitlines(keepends=True)
  source_lines = [l for l in all_lines if l.startswith(';')]
  ret = []
  if source_lines:
    ret = ['Showing source lines that appear in the symbol (via objdump).\n']
  else:
    ret = [
        'Symbol missing source lines. Showing raw disassembly (via objdump).\n'
    ]
  lines = source_lines or all_lines
  if len(lines) > 10:
    ret += ['This might be verbose due to inlined functions.\n']
  ret += lines
  return ''.join(ret)


def _DumpInitArray(binary):
  cmd = [_TOOLCHAIN_PREFIX + 'readobj', '--hex-dump=.init_array', binary]
  output = subprocess.check_output(cmd, encoding='utf8')
  # Example output:
  # File: lib.unstripped/libmonochrome_64.so
  # Format: elf64-littleaarch64
  # Arch: aarch64
  # AddressSize: 64bit
  # LoadName: libmonochrome_64.so
  # Hex dump of section '.init_array':
  # 0x091f6198 14f80204 00000000 c0cf3003 00000000 ..........0.....
  # 0x091f61a8 68c70104 00000000                   h........^F.....
  is_64_bit = False
  is_arm = False
  byte_order = 'little'
  ret = []
  for line in output.splitlines():
    if line.startswith('Format:') and 'big' in line:
      byte_order = 'big'
      continue
    if line == 'Arch: arm':
      is_arm = True
      continue
    if line == 'AddressSize: 64bit':
      is_64_bit = True
      continue
    if not line.startswith('0x'):
      continue
    init_array_address = int(line[:10], 16)
    parts = line[10:-16].split()
    assert len(parts) <= 4, 'Too many parts: ' + line
    if is_64_bit:
      parts = [parts[i] + parts[i + 1] for i in range(0, len(parts), 2)]
    arrays = (bytearray.fromhex(p) for p in parts)
    for a in arrays:
      address = int.from_bytes(a, byteorder=byte_order, signed=False)
      if is_arm:
        address = address & ~1  # Adjust for arm thumb addresses being odd.
      ret.append((init_array_address, address))
      init_array_address += 8 if is_64_bit else 4
  return ret


def _DumpRelativeRelocations(binary):
  # Example output from: llvm-readobj --relocations chrome
  # File: chrome
  # Format: elf64-x86-64
  # Arch: x86_64
  # AddressSize: 64bit
  # LoadName: <Not found>
  # Relocations [
  #   Section (10) .rela.dyn {
  #     0x26C2AD88 R_X86_64_RELATIVE - 0xA6DABE0
  #     0x26C2AD90 R_X86_64_RELATIVE - 0xA6DC2B0
  # ...
  cmd = [_TOOLCHAIN_PREFIX + 'readobj', '--relocations', binary]
  lines = subprocess.check_output(cmd, encoding='utf8').splitlines()
  ret = {}
  for line in lines:
    if 'RELATIVE' in line:
      parts = line.split()
      ret[int(parts[0], 16)] = int(parts[-1], 16)
  return ret


def _ResolveRelativeAddresses(binary, address_tuples):
  relocations_dict = None
  ret = []
  for init_address, address in address_tuples:
    if address == 0:
      if relocations_dict is None:
        relocations_dict = _DumpRelativeRelocations(binary)
      address = relocations_dict.get(init_address)
      if address is None:
        raise Exception('Failed to resolve relocation for address: ' +
                        hex(init_address))
    ret.append(address)
  return ret


def _SymbolizeAddresses(binary, addresses):
  # Example output from: llvm-symbolizer -e chrome \
  #    --output-style=JSON --functions 0x3323430 0x403a768 0x5489b98
  # [{"Address":"0xa6afdd0","ModuleName":"chrome","Symbol":[...]}, ...]
  # Where Symbol = {"Column":24,"Discriminator":0,"FileName":"...",
  #    "FunctionName":"MaybeStartBackgroundThread","Line":85,
  #    "StartAddress":"0xa6afdd0","StartFileName":"","StartLine":0}
  ret = {}
  if not addresses:
    return ret
  cmd = [
      _TOOLCHAIN_PREFIX + 'symbolizer', '-e', binary, '--functions',
      '--output-style=JSON'
  ] + [hex(a) for a in addresses]
  output = subprocess.check_output(cmd, encoding='utf8')
  for main_entry in json.loads(output):
    # Multiple symbol entries can exist due to inlining. Last entry is the
    # outer-most symbol.
    symbols = main_entry['Symbol']
    name_entry = symbols[-1]
    # Take the last entry that has a line number as the best filename.
    file_entry = next((x for x in symbols[::-1] if x['Line'] != 0), name_entry)
    address = int(main_entry['Address'], 16)
    filename = file_entry['FileName']
    line = file_entry['Line']
    if line:
      filename += f':{line}'
    ret[address] = (filename, name_entry['FunctionName'])
  return ret


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--json',
                      action='store_true',
                      help='Output in JSON format')
  parser.add_argument('binary', help='The non-stripped binary to analyze.')
  args = parser.parse_args()

  address_tuples = _DumpInitArray(args.binary)
  addresses = _ResolveRelativeAddresses(args.binary, address_tuples)
  symbolized_by_address = _SymbolizeAddresses(args.binary, addresses)

  skip_disassembly = len(addresses) > _MAX_DISASSEMBLY_SYMBOLS
  if skip_disassembly:
    sys.stderr.write('Not collection disassembly due to the large number of '
                     'results.\n')
  else:
    size_by_address = _ParseNm(args.binary, addresses)

  entries = []
  for address in addresses:
    filename, symbol_name = symbolized_by_address[address]
    if skip_disassembly:
      disassembly = ''
    else:
      size = size_by_address.get(address, 0)
      if size == 0:
        disassembly = ('Not showing disassembly because of unknown symbol size '
                       '(assembly symbols sometimes omit size).\n')
      else:
        disassembly = _Disassemble(args.binary, address, address + size)
    entries.append({
        'address': address,
        'disassembly': disassembly,
        'filename': filename,
        'symbol_name': symbol_name,
    })

  if args.json:
    print(json.dumps({'entries': entries}))
    return

  for e in entries:
    print(f'# 0x{e["address"]:x} {e["filename"]} {e["symbol_name"]}')
    print(e['disassembly'])

  print(f'Found {len(entries)} files containing static initializers.')


if '__main__' == __name__:
  main()
