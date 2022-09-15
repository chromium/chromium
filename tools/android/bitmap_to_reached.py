#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""From several dumps of reached offsets, create the list of reached symbols."""

import argparse
import logging
import os
import struct
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))
path = os.path.join(_SRC_PATH, 'tools', 'cygprofile')
sys.path.append(path)
import process_profiles

SIZEOF_INT = 4
BITS_IN_INT = SIZEOF_INT * 8
# This is the number of bytes represented in a dump by one bit.
BYTES_GRANULARITY = 4

def _DumpToOffsets(filename):
  """From a dump, returns a list of offsets in it.

  Args:
    filename: (str) Dump filename.

  Returns:
    ([int]) offsets in the dump, that is relative to .text start.
  """
  bitfield = None
  offsets = []
  with open(filename, 'r') as f:
    bitfield = f.read()
  assert len(bitfield) % SIZEOF_INT == 0
  count = len(bitfield) / SIZEOF_INT
  for i in xrange(count):
    entry = struct.unpack_from('<I', bitfield, offset=i * SIZEOF_INT)[0]
    for bit in range(BITS_IN_INT):
      if entry & (1 << bit):
        offsets.append((i * BITS_IN_INT + bit) * BYTES_GRANULARITY)
  return offsets


def _ReachedSymbols(offsets, offset_to_symbol):
  """Returns a list of reached symbols from offsets in .text.

  Args:
    offsets: ([int]) List of reached offsets.
    offset_to_symbol: [symbol_extractor.SymbolInfo or None] as returned by
      |SymbolOffsetProcessor.GetDumpOffsetToSymbolInfo()|

  Returns:
    ([symbol_extractor.SymbolInfo])
  """
  symbol_infos = set()
  missing = 0
  for offset in offsets:
    # |offset_to_symbol| has one entry per BYTES_GRANULARITY bytes.
    index = offset / BYTES_GRANULARITY
    if index > len(offset_to_symbol):
      missing += 1
      continue
    symbol_infos.add(offset_to_symbol[index])
  if missing:
    logging.warning('Couldn\'t match %d symbols', missing)
  return symbol_infos


def _CreateArgumentParser():
  """Returns an ArgumentParser."""
  parser = argparse.ArgumentParser(description='Outputs reached symbols')
  parser.add_argument('--build-dir', type=str,
                      help='Path to the build directory', required=True)
  parser.add_argument('--dumps', type=str, help='A comma-separated list of '
                      'files with instrumentation dumps', required=False)
  parser.add_argument('--dumps-dir', type=str, help='Directory name with'
                      'reached code dumps', required=False)
  parser.add_argument('--library-name', default='libchrome.so',
                      help=('Chrome shared library name (usually libchrome.so '
                            'or libmonochrome.so'))
  parser.add_argument('--output', required=True, help='Output filename')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = _CreateArgumentParser()
  args = parser.parse_args()
  dumps = []
  if args.dumps:
    dumps.extend(args.dumps.split(','))
  elif args.dumps_dir:
    for file_name in os.listdir(args.dumps_dir):
      dumps.append(os.path.join(args.dumps_dir, file_name))
  else:
    logging.error('Either --dumps or --dumps-dir must be provided')
    parser.print_help()
    return 1
  logging.info('Parsing dumps')
  offsets = set()
  for dump_filename in dumps:
    offsets |= set(_DumpToOffsets(dump_filename))
  logging.info('Found %d reached locations', len(offsets))
  library_path = os.path.join(args.build_dir, 'lib.unstripped',
                              args.library_name)
  processor = process_profiles.SymbolOffsetProcessor(library_path)
  logging.info('Finding Symbols')
  offset_to_symbol = processor.GetDumpOffsetToSymbolInfo()
  reached_symbol_infos = _ReachedSymbols(offsets, offset_to_symbol)
  reached_symbol_infos.remove(None)
  with open(args.output, 'w') as f:
    for s in reached_symbol_infos:
        f.write('%s\n' % s.name)

  # Print some stats.
  reached_size = sum(s.size for s in reached_symbol_infos)
  logging.info('Total reached size = {}'.format(reached_size))
  all_symbol_infos = set()
  for i in offset_to_symbol:
    if i is not None:
      all_symbol_infos.add(i)
  total_size = sum(s.size for s in all_symbol_infos)
  logging.info('Total size of known symbols = {}'.format(total_size))
  coverage_percent = float(reached_size) / total_size * 100;
  logging.info('Coverage: {0:.2f}%'.format(coverage_percent))


if __name__ == '__main__':
  main()
  sys.exit(0)

