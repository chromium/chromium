#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs dwarfdump on passed-in .so."""

import argparse
import bisect
import dataclasses
import logging
import os
import re
import subprocess
import typing

import path_util


_DWARF_DUMP_FLAGS = ['--debug-info', '--recurse-depth=0']

# Matching and group examples:
# '0x00001234: DW_TAG_compile_unit' -> None
# '  DW_AT_low_pc  (0x123)' -> ('DW_', None)
# '  DW_AT_name  ("foo")' -> ('DW_', 'foo')
_RE_DW_AT_NAME = re.compile(r'\s+(DW_)(?:AT_name\s+\("(.*?)"\))?')


class _DwoNameLookup:
  """Helper to look up name (source file) from .dwo files

  dwarfdump of an ELF file normally specifies source files in DW_AT_name fields.
  However, debug fission can move debug info from ELF files to .dwo files. In
  this case, dwarfdump would omit DW_AT_name of affected symbols, and use
  DW_AT_GNU_dwo_name to specify the path (relative to output dir) of the
  matching .dwo files, whose dwarfdump would then specify the matching source
  file in DW_AT_name.

  This class performs cached lookup from .dwo to name (source file).
  """

  def __init__(self, any_path):
    finder = path_util.OutputDirectoryFinder(
        any_path_within_output_directory=any_path)
    self._output_path = finder.Detect()  # May be None.
    self._dwarf_dump_path = path_util.GetDwarfdumpPath()
    self._cache = {}

  def _ReadName(self, dwo_path):
    """Runs dwarfdump on .dwo to extract name.

    If this is not possible then returns |dwo_path|.
    """
    if self._output_path is None:
      return dwo_path
    # Assumption: |dwo_path| is relative to output path.
    real_dwo_path = os.path.join(self._output_path, dwo_path)
    cmd = [self._dwarf_dump_path, real_dwo_path] + _DWARF_DUMP_FLAGS
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL,
                            encoding='utf-8')
    name = None
    state = 0
    # Scan output line by line, exit and terminate as soon as possible.
    for line in iter(proc.stdout.readline, ''):
      if state == 0:  # Scan for DW_TAG_compile_unit.
        if 'DW_TAG_compile_unit' in line:
          state = 1
      elif state == 1:  # scan for DW_AT_name.
        m = _RE_DW_AT_NAME.match(line)
        if not m:  # Not even matching prefix '  DW_'.
          break
        name = m.groups()[1]
        if name is not None:  # Extracted names.
          break
        # Else matches '  DW_': Continue scanning.
    proc.kill()
    return dwo_path if name is None else name

  def Lookup(self, dwo_path):
    """Looks up name in .dwo, with caching."""
    if dwo_path in self._cache:
      name = self._cache[dwo_path]
    else:
      name = self._ReadName(dwo_path)
      self._cache[dwo_path] = name
    return name

  def LogStats(self):
    if self._cache:
      num_success = sum(1 for k, v in self._cache.items() if k != v)
      logging.info('Successful .dwo lookups: %d / %d', num_success,
                   len(self._cache))


@dataclasses.dataclass(order=True)
class _AddressRange:
  start: int
  stop: int


class _SourceMapper:
  def __init__(self, range_info_list):
    self._range_info_list = range_info_list
    self._largest_address = 0

    if self._range_info_list:
      self._largest_address = self._range_info_list[-1][0].stop

  def FindSourceForTextAddress(self, address):
    """Returns source file path matching passed-in symbol address.

    Only symbols in the .text section of the elf file are supported.
    """
    # Bisect against stop = self._largest_address + 1 to avoid bisecting against
    # the "source path" tuple component.
    bisect_index = bisect.bisect_right(
        self._range_info_list,
        (_AddressRange(address, self._largest_address + 1), '')) - 1
    if bisect_index >= 0:
      info = self._range_info_list[bisect_index]
      if info[0].start <= address < info[0].stop:
        return info[1]

    return ''

  def NumberOfPaths(self):
    return len(set(info[1] for info in self._range_info_list))

  @property
  def num_ranges(self):
    return len(self._range_info_list)


def CreateAddressSourceMapper(elf_path):
  """Runs dwarfdump. Returns object for querying source path given address."""
  return _SourceMapper(_Parse(elf_path))


def CreateAddressSourceMapperForTest(lines, dwo_name_lookup=None):
  return _SourceMapper(_ParseDumpOutput(lines, dwo_name_lookup))


def ParseDumpOutputForTest(lines, dwo_name_lookup=None):
  return _ParseDumpOutput(lines, dwo_name_lookup)


def _Parse(elf_path):
  cmd = [path_util.GetDwarfdumpPath(), elf_path] + _DWARF_DUMP_FLAGS
  logging.debug('Running: %s', ' '.join(cmd))
  stdout = subprocess.check_output(cmd,
                                   stderr=subprocess.DEVNULL,
                                   encoding='utf-8')
  return _ParseDumpOutput(stdout.splitlines(), _DwoNameLookup(elf_path))


def _ParseDumpOutput(lines, dwo_name_lookup=None):
  """Parses passed-in dwarfdump stdout."""

  # List of (_AddressRange, source path) tuples.
  range_info_list = []

  line_it = iter(lines)
  line = next(line_it, None)
  while line is not None:
    if 'DW_TAG_compile_unit' not in line:
      line = next(line_it, None)
      continue

    line, address_ranges, source_path, dwo_path = _ParseCompileUnit(line_it)
    if (source_path or dwo_path) and address_ranges:
      for address_range in address_ranges:
        if dwo_path:
          source_path = (dwo_name_lookup.Lookup(dwo_path)
                         if dwo_name_lookup else dwo_path)
        range_info_list.append((address_range, source_path))

  if dwo_name_lookup:
    dwo_name_lookup.LogStats()

  return sorted(range_info_list)


def _ParseCompileUnit(line_it):
  """Parses DW_AT_compile_unit block.

  Example:
  0x000026: DW_AT_compile_unit
              DW_AT_low_pc  (0x02f)
              DW_AT_high_pc  (0x03f)
              DW_AT_name  ("foo.cc")
              DW_AT_GNU_dwo_name  ("foo.dwo")
  """
  source_path = None
  dwo_path = None
  single_range = _AddressRange(0, 0)
  range_addresses = []

  while True:
    line = next(line_it, None)

    dw_index = 0 if line is None else line.find('DW_')
    if dw_index < 0:
      continue

    if line is None or line.startswith('DW_TAG', dw_index):
      if range_addresses:
        # If compile unit specifies both DW_AT_ranges and DW_AT_low_pc,
        # DW_AT_low_pc is base offset. Base offset is currently unsupported.
        assert single_range.start == 0
      elif single_range.start > 0:
        range_addresses.append(single_range)
      return (line, range_addresses, source_path, dwo_path)

    if line.startswith('DW_AT_low_pc', dw_index):
      single_range.start = int(_ExtractDwValue(line), 16)
      if single_range.stop == 0:
        single_range.stop = single_range.start + 1
    elif line.startswith('DW_AT_high_pc', dw_index):
      single_range.stop = int(_ExtractDwValue(line), 16)
    elif line.startswith('DW_AT_name', dw_index):
      source_path = _ExtractDwValue(line)
    elif line.startswith('DW_AT_GNU_dwo_name', dw_index):
      dwo_path = _ExtractDwValue(line)
    elif line.startswith('DW_AT_ranges', dw_index):
      range_addresses = _ParseRanges(line_it)


def _ParseRanges(line_it):
  """Parses DW_AT_ranges from dwarfdump stdout.

  Example:
  [0x1, 0x2)
  [0x5, 0x10))
  """
  range_addresses = []

  line = next(line_it, None)
  while line is not None:
    num_opening_brackets = line.count('(') + line.count('[')
    num_closing_brackets = line.count(')') + line.count(']')

    tokens = line.strip('([]) \t').split(',')
    if len(tokens) == 2:
      start_address = int(tokens[0], 16)
      end_address = int(tokens[1], 16)
      # Dwarf spec does not assign special meaning to empty ranges.
      if start_address != end_address:
        range_addresses.append(_AddressRange(start_address, end_address))

    if num_closing_brackets > num_opening_brackets:
      break
    line = next(line_it, None)

  return range_addresses


def _ExtractDwValue(line):
  """Extract DW_AT_ value from dwarfdump stdout.

  Examples:
  DW_AT_name  ("foo.cc")
  DW_AT_decl_line  (177)
  DW_AT_low_pc  (0x2)
  """
  lparen_index = line.rfind('(')
  if lparen_index < 0:
    return None
  rparen_index = line.find(')', lparen_index + 1)
  if rparen_index < 0:
    return None
  if (lparen_index < rparen_index - 2 and line[lparen_index + 1] == '"'
      and line[rparen_index - 1] == '"'):
    lparen_index += 1
    rparen_index -= 1
  return line[lparen_index + 1:rparen_index]


def main():
  parser = argparse.ArgumentParser()
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument('--dwarf-dump-output', type=os.path.realpath)
  group.add_argument('--elf-file', type=os.path.realpath)

  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  if args.dwarf_dump_output:
    dwo_name_lookup = _DwoNameLookup(args.dwarf_dump_output)
    with open(args.dwarf_dump_output, 'r') as f:
      source_mapper = CreateAddressSourceMapperForTest(f.read().splitlines(),
                                                       dwo_name_lookup)
  else:
    assert args.elf_file
    source_mapper = CreateAddressSourceMapper(args.elf_file)
  logging.warning('Found %d source paths across %d ranges',
                  source_mapper.NumberOfPaths(), source_mapper.num_ranges)


if __name__ == '__main__':
  main()
