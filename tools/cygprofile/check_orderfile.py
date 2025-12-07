#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check that symbols are ordered into a binary as they appear in the orderfile.
"""

import argparse
import dataclasses
import logging
import pathlib
import sys
from typing import Dict
from typing import Set

import orderfile_shared
import symbol_extractor

_DEFAULT_THRESHOLD = 80


@dataclasses.dataclass
class _OffsetGroup:
  """Represents a set of symbols sharing the same offset."""
  offset: int
  names: Set[str] = dataclasses.field(default_factory=set)
  used: bool = False


def _NamesToOffsetGroups(
    symbol_infos: symbol_extractor.SymbolInfo) -> Dict[str, _OffsetGroup]:
  """Groups all symbols by offset."""
  name_to_group = {}
  offset_to_group = {}
  for si in symbol_infos:
    if not si.offset in offset_to_group:
      group = _OffsetGroup(si.offset)
      offset_to_group[si.offset] = group
    group = offset_to_group[si.offset]
    group.names.add(si.name)
    name_to_group[si.name] = group
  return name_to_group


def _MultipleAddressSymbols(symbol_infos: symbol_extractor.SymbolInfo,
                            name_to_group: Dict[str, _OffsetGroup]):
  """Finds a set of symbols appearing at the same address."""
  ret = set()
  for si in symbol_infos:
    if (si.name in name_to_group
        and si.offset != name_to_group[si.name].offset):
      ret.add(si.name)
  return ret


def _VerifySymbolOrder(orderfile_symbols, symbol_infos, threshold):
  """Verify symbol ordering.

  Checks that the non-section symbols in |orderfile_filename| are consistent
  with the offsets |symbol_infos|.

  Args:
    orderfile_symbols: ([str]) list of symbols from orderfile.
    symbol_infos: ([SymbolInfo]) symbol infos from binary.
    threshold: (int) The number of misordered symbols beyond which we error.

  Returns:
    True iff the ordering is consistent within |threshold|.
  """
  name_to_group = _NamesToOffsetGroups(symbol_infos)
  symbols_at_multiple_addresses = _MultipleAddressSymbols(
      symbol_infos, name_to_group)

  # Verify that offsets of the symbols from the orderfile appear in the binary
  # in non-decreasing order.
  last_offset = 0
  name_to_offset = {si.name: si.offset for si in symbol_infos}
  missing_count = 0
  missing_syms = []
  misorder_count = 0
  misordered_syms = []
  for sym in orderfile_symbols:
    if sym in symbols_at_multiple_addresses:
      logging.warning(
          'Ignoring symbol from orderfile appearing ' +
          'at multiple addresses: %s', sym)
      continue
    if sym not in name_to_group:
      missing_count += 1
      missing_syms.append(sym)
      continue
    next_offset = name_to_offset[sym]
    group = name_to_group[sym]
    if group.used:
      # Once a group of offsets appears in the orderfile, all symbols from the
      # same group further down in the orderfile are ignored.
      continue
    group.used = True
    next_offset = group.offset
    if next_offset < last_offset:
      misorder_count += 1
      misordered_syms.append((sym, next_offset, last_offset))
    last_offset = next_offset

  # Report missing and incorrectly ordered symbols.
  if missing_count:
    # TODO(crbug.com/340534475): Return False when too many symbols are
    # missing.
    logging.warning('%d missing symbols:\n%s', missing_count,
                    '\n'.join(str(x) for x in missing_syms[:100]))
  if misorder_count:
    logging.warning('%d misordered symbols:\n%s', misorder_count,
                    '\n'.join(str(x) for x in misordered_syms[:threshold]))
    if misorder_count > threshold:
      logging.error('%d misordered symbols over threshold %d, failing',
                    misorder_count, threshold)
      return False
  return True


def ExtractAndVerifySymbolOrder(binary_filename: str,
                                orderfile_filename: str,
                                threshold: int = _DEFAULT_THRESHOLD):
  """Extracts symbols and verifies the ordering.

  Args:
    binary_filename: Path to the binary.
    orderfile_filename: Path to the orderfile.
    threshold: The number of misordered symbols beyond which we error.

  Returns:
    True iff the ordering is consistent within |threshold|.
  """
  symbol_infos = symbol_extractor.SymbolInfosFromBinary(binary_filename)
  with open(orderfile_filename, 'r') as f:
    orderfile_symbols = [s.strip() for s in f]
  return _VerifySymbolOrder(orderfile_symbols, symbol_infos, threshold)


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-arch',
                      dest='arch',
                      required=True,
                      choices=['arm', 'arm64', 'x86', 'x64'],
                      help='The target architecture for which to build.')
  parser.add_argument('-C',
                      '--out-dir',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the output directory.')
  parser.add_argument('--orderfile-path',
                      required=True,
                      help='Path to the orderfile to verify.')
  parser.add_argument(
      '--threshold',
      action='store',
      dest='threshold',
      default=_DEFAULT_THRESHOLD,
      type=int,
      help='The maximum allowed number of out-of-order symbols.')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbosity',
                      action='count',
                      default=0,
                      help='Increase verbosity for debugging.')
  return parser


def main():
  parser = CreateArgumentParser()
  options = parser.parse_args()
  if options.verbosity >= 1:
    level = logging.DEBUG
  else:
    level = logging.INFO
  logging.basicConfig(level=level,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  lib_chrome_so = orderfile_shared.GetLibchromeSoPath(options.out_dir,
                                                      options.arch)
  if ExtractAndVerifySymbolOrder(lib_chrome_so, options.orderfile_path,
                                 options.threshold):
    return 0
  return 1


if __name__ == '__main__':
  sys.exit(main())
