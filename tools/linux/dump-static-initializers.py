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
import os
import pathlib
import re
import subprocess
import sys

_TOOLCHAIN_PREFIX = str(
    pathlib.Path(__file__).parents[2] / 'third_party' / 'llvm-build' /
    'Release+Asserts' / 'bin' / 'llvm-')


# Regex matching nm output for the symbols we're interested in. The two formats
# we are interested in are _GLOBAL__sub_I_<filename> and _cxx_global_var_initN.
# See test_ParseNmLine for examples.
nm_re = re.compile(
    r'''(\S+)\s(\S+)\st\s                # Symbol start address and size
        (
          (?:_ZN12)?_GLOBAL__(?:sub_)?I_ # Pattern with filename
        |
          __cxx_global_var_init\d*       # Pattern without filename
        )(.*)                            # capture the filename''',
    re.X)
def ParseNmLine(line):
  """Parse static initializers from a line of nm output.

  Given a line of nm output, parse static initializers as a
  (file, start, size, symbol) tuple."""
  match = nm_re.match(line)
  if match:
    addr, size, prefix, filename = match.groups()
    return (filename, int(addr, 16), int(size, 16), prefix+filename)
  return None


def test_ParseNmLine():
  """Verify the nm_re regex matches some sample lines."""
  parse = ParseNmLine(
    '0000000001919920 0000000000000008 t '
    '_ZN12_GLOBAL__I_safe_browsing_service.cc')
  assert parse == ('safe_browsing_service.cc', 26319136, 8,
                   '_ZN12_GLOBAL__I_safe_browsing_service.cc'), parse

  parse = ParseNmLine(
    '00000000026b9eb0 0000000000000024 t '
    '_GLOBAL__sub_I_extension_specifics.pb.cc')
  assert parse == ('extension_specifics.pb.cc', 40607408, 36,
                   '_GLOBAL__sub_I_extension_specifics.pb.cc'), parse

  parse = ParseNmLine(
    '0000000002e75a60 0000000000000016 t __cxx_global_var_init')
  assert parse == ('', 48716384, 22, '__cxx_global_var_init'), parse

  parse = ParseNmLine(
    '0000000002e75a60 0000000000000016 t __cxx_global_var_init89')
  assert parse == ('', 48716384, 22, '__cxx_global_var_init89'), parse


# Just always run the test; it is fast enough.
test_ParseNmLine()


def _ParseNm(binary):
  output = subprocess.check_output(
      [_TOOLCHAIN_PREFIX + 'nm', '--print-size', binary], encoding='utf8')
  ret = {}
  for line in output.splitlines():
    parse = ParseNmLine(line)
    if parse:
      _, address, size, _ = parse
      ret[address] = size
  return ret


def _Disassemble(binary, start, end):
  """Given a span of addresses, returns symbol references from disassembly."""
  cmd = [
      _TOOLCHAIN_PREFIX + 'objdump',
      binary,
      '--disassemble',
      '--source',
      '--demangle',
      '--start-address=0x%x' % start,
      '--stop-address=0x%x' % end,
  ]
  lines = subprocess.check_output(cmd, encoding='utf8').splitlines()
  source_lines = [l for l in lines if l.startswith(';')]
  return bool(source_lines), '\n'.join(source_lines or lines)


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
  byte_order = 'little'
  ret = []
  for line in output.splitlines():
    if line.startswith('Format:') and 'big' in line:
      byte_order = 'big'
      continue
    if line == 'AddressSize: 64bit':
      is_64_bit = True
      continue
    if not line.startswith('0x'):
      continue
    parts = line[10:-16].split()
    assert len(parts) <= 4, 'Too many parts: ' + line
    if is_64_bit:
      parts = [parts[i] + parts[i + 1] for i in range(0, len(parts), 2)]
    arrays = (bytearray.fromhex(p) for p in parts)
    ret.extend(
        int.from_bytes(a, byteorder=byte_order, signed=False) for a in arrays)
  return ret


def _SymbolizeAddresses(binary, addresses):
  # Example output from: llvm-symbolizer -e lib.unstripped/libmonochrome_64.so \
  #    --functions 0x3323430 0x403a768 0x5489b98
  # _GLOBAL__I_000100
  # ./../../buildtools/third_party/libc++/trunk/src/iostream.cpp:0:0
  #
  # _GLOBAL__sub_I_base_logging.cc
  # ./../../third_party/gvr_shim/src/geo/render/ion/base/base_logging.cc:0:0
  #
  # _GLOBAL__sub_I_token.cc
  # ./../../v8/src/parsing/token.cc:0:0
  cmd = [_TOOLCHAIN_PREFIX + 'symbolizer', '-e', binary, '--functions'
         ] + [hex(a) for a in addresses]
  output = subprocess.check_output(cmd, encoding='utf8').splitlines()
  lines = iter(output)
  ret = {}
  for address in addresses:
    symbol_name = next(lines)
    filename = next(lines)
    blank_line = next(lines)
    if blank_line:
      raise Exception(f'Should have been blank: {blank_line}\nOutput: {output}')
    ret[address] = (filename, symbol_name)
  return ret


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-d',
                      '--diffable',
                      dest='diffable',
                      action='store_true',
                      default=False,
                      help='Prints the filename on each line, for more easily '
                      'diff-able output. (Used by sizes.py)')
  parser.add_argument('-t', '--toolchain-prefix', help='Unused.')
  parser.add_argument('binary', help='The non-stripped binary to analyze.')
  args = parser.parse_args()

  addresses = _DumpInitArray(args.binary)
  symbolized_by_address = _SymbolizeAddresses(args.binary, addresses)
  if not args.diffable:
    size_by_address = _ParseNm(args.binary)

  for address in addresses:
    if address == 0:
      # See //docs/static_initializers.md#Step-3-Manual-Verification
      print('UNIMPLEMENTED: Support for .init_array entries with reloctions')
      continue
    filename, symbol_name = symbolized_by_address[address]
    print('# %s (address=0x%x name=%s)' % (filename, address, symbol_name))
    if args.diffable:
      continue

    size = size_by_address.get(address, 0)

    if size == 0:
      print('nm output missing for this symbol!')
    elif size == 2:
      # gcc generates a two-byte 'repz retq' initializer when there is a
      # ctor even when the ctor is empty.  This is fixed in gcc 4.6, but
      # Android uses gcc 4.4.
      print('[empty ctor, but it still has cost on gcc <4.6]')
    else:
      has_source, disassembly = _Disassemble(args.binary, address,
                                             address + size)
      if has_source:
        print('Showing source lines that appear in the symbol (via objdump).')
      else:
        print('Symbol missing source lines, so showing raw disassembly (via '
              'objdump).')
      print('This might be verbose due to inlined functions.')
      print(disassembly)
    print()

  if args.diffable:
    print('#', end=' ')
  print(f'Found {len(addresses)} files containing static initializers.')


if '__main__' == __name__:
  main()
