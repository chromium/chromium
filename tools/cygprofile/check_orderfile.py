#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check that symbols are ordered into a binary as they appear in the orderfile.
"""

import logging
import optparse
import sys

import symbol_extractor


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
  last_offset = 0
  name_to_offset = {si.name: si.offset for si in symbol_infos}
  missing_count = 0
  misorder_count = 0
  misordered_syms = []
  for sym in orderfile_symbols:
    if '.' in sym:
      continue  # sym is a section name.
    if sym not in name_to_offset:
      missing_count += 1
      continue
    next_offset = name_to_offset[sym]
    if next_offset < last_offset:
      misorder_count += 1
      misordered_syms.append((sym, next_offset, last_offset))
    last_offset = next_offset
  logging.warning('Missing symbols in verification: %d', missing_count)
  if misorder_count:
    logging.warning('%d misordered symbols:\n %s', misorder_count,
                    '\n '.join(str(x) for x in misordered_syms[:threshold]))
    if misorder_count > threshold:
      logging.error('%d misordered symbols over threshold %d, failing',
                    misorder_count, threshold)
      return False
  return True


def main():
  parser = optparse.OptionParser(usage=
      'usage: %prog [options] <binary> <orderfile>')
  parser.add_option('--target-arch', help='Unused')
  parser.add_option('--threshold',
                    action='store',
                    dest='threshold',
                    default=80,
                    type=int,
                    help='The maximum allowed number of out-of-order symbols.')
  options, argv = parser.parse_args(sys.argv)
  if len(argv) != 3:
    parser.print_help()
    return 1
  (binary_filename, orderfile_filename) = argv[1:]

  symbol_infos = symbol_extractor.SymbolInfosFromBinary(binary_filename)

  if not _VerifySymbolOrder(
      [sym.strip() for sym in open(orderfile_filename, 'r')], symbol_infos,
      options.threshold):
    return 1
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
