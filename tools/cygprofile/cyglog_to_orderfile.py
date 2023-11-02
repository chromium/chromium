#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Symbolizes a log file produced by cyprofile instrumentation.

Given a log file and the binary being profiled, creates an orderfile.
"""

import argparse
import logging
import multiprocessing
import os
import re
import sys
import tempfile

import cygprofile_utils
import process_profiles
import symbol_extractor


class _SymbolNotFoundException(Exception):
  """Exception used during internal processing."""
  def __init__(self, value):
    super().__init__(value)
    self.value = value

  def __str__(self):
    return repr(self.value)


def GetObjectFilenames(obj_dir):
  """Returns all a list of .o files in a given directory tree."""
  obj_files = []
  # Scan _obj_dir recursively for .o files.
  for (dirpath, _, filenames) in os.walk(obj_dir):
    for file_name in filenames:
      if file_name.endswith('.o'):
        obj_files.append(os.path.join(dirpath, file_name))
  return obj_files


class ObjectFileProcessor:
  """Processes symbols found in the object file tree.

  This notably includes the section names of each symbol, as well as component
  information that can be taken from the directory (not yet implemented).
  """
  def __init__(self, obj_dir):
    """Initialize.

    Args:
      obj_dir (str) The root of the object directory.
    """
    self._obj_dir = obj_dir
    self._symbol_to_sections_map = None

  def GetSymbolToSectionsMap(self):
    """Scans object files to find symbol section names.

    Returns:
      {symbol: linker section(s)}
    """
    if self._symbol_to_sections_map is not None:
      return self._symbol_to_sections_map

    symbol_infos = self._GetAllSymbolInfos()
    self._symbol_to_sections_map = {}
    symbol_warnings = cygprofile_utils.WarningCollector(300)
    for symbol_info in symbol_infos:
      symbol = symbol_info.name
      if symbol.startswith('.LTHUNK'):
        continue
      section = symbol_info.section
      if ((symbol in self._symbol_to_sections_map) and
          (symbol_info.section not in self._symbol_to_sections_map[symbol])):
        self._symbol_to_sections_map[symbol].append(section)

        if not self._SameCtorOrDtorNames(
            symbol, self._symbol_to_sections_map[symbol][0].lstrip('.text.')):
          symbol_warnings.Write('Symbol ' + symbol +
                                ' unexpectedly in more than one section: ' +
                                ', '.join(self._symbol_to_sections_map[symbol]))
      elif not section.startswith('.text.'):
        # Assembly functions have section ".text". These are all grouped
        # together near the end of the orderfile via an explicit ".text" entry.
        if section != '.text':
          symbol_warnings.Write('Symbol ' + symbol +
                                ' in incorrect section ' + section)
      else:
        # In most cases we expect just one item in this list, and maybe 4 or so
        # in the worst case.
        self._symbol_to_sections_map[symbol] = [section]
    symbol_warnings.WriteEnd('bad sections')
    return self._symbol_to_sections_map

  @classmethod
  def _SameCtorOrDtorNames(cls, symbol1, symbol2):
    """Returns True if two symbols refer to the same constructor or destructor.

    The Itanium C++ ABI specifies dual constructor and destructor emmission
    (section 5.1.4.3):
    https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling-special-ctor-dtor
    To avoid fully parsing all mangled symbols, a heuristic is used with
    c++filt.

    Note: some compilers may name generated copies differently.  If this becomes
    an issue this heuristic will need to be updated.
    """
    # Check if this is the understood case of constructor/destructor
    # signatures. GCC emits up to three types of constructor/destructors:
    # complete, base, and allocating.  If they're all the same they'll
    # get folded together.
    return (re.search('(C[123]|D[012])E', symbol1) and
            symbol_extractor.DemangleSymbol(symbol1) ==
            symbol_extractor.DemangleSymbol(symbol2))

  def _GetAllSymbolInfos(self):
    obj_files = GetObjectFilenames(self._obj_dir)
    pool = multiprocessing.Pool()
    # Hopefully the object files are in the page cache at this point as
    # typically the library has just been built before the object files are
    # scanned. Hence IO should not be a problem, and there is no need for a
    # concurrency limit on the pool.
    symbol_infos_nested = pool.map(
        symbol_extractor.SymbolInfosFromBinary, obj_files)
    pool.close()
    pool.join()
    result = []
    for symbol_infos in symbol_infos_nested:
      result += symbol_infos
    return result


class OffsetOrderfileGenerator:
  """Generates an orderfile from instrumented build offsets.

  The object directory, SymbolOffsetProcessor and dump offsets must all be from
  the same build.
  """
  def __init__(self, symbol_offset_processor, obj_processor):
    """Initialize the generator.

    Args:
      symbol_offset_processor (process_profiles.SymbolOffsetProcessor).
      obj_processor (ObjectFileProcessor).
    """
    self._offset_to_symbols = symbol_offset_processor.OffsetToSymbolsMap()
    self._obj_processor = obj_processor

  def GetOrderedSections(self, offsets):
    """Get ordered list of sections corresponding to offsets.

    Args:
      offsets ([int]) dump offsets, from same build as parameters to __init__.

    Returns:
      [str] ordered list of sections suitable for use as an orderfile, or
            None if failure.
    """
    symbol_to_sections = self._obj_processor.GetSymbolToSectionsMap()
    if not symbol_to_sections:
      logging.error('No symbol section names found')
      return None
    unknown_symbol_warnings = cygprofile_utils.WarningCollector(300)
    symbol_not_found_errors = cygprofile_utils.WarningCollector(
        300, level=logging.ERROR)
    seen_sections = set()
    ordered_sections = []
    success = True
    for offset in offsets:
      try:
        symbol_infos = self._SymbolsAtOffset(offset)
        for symbol_info in symbol_infos:
          if symbol_info.name in symbol_to_sections:
            sections = symbol_to_sections[symbol_info.name]
            for section in sections:
              if not section in seen_sections:
                ordered_sections.append(section)
                seen_sections.add(section)
          else:
            unknown_symbol_warnings.Write(
                'No known section for symbol ' + symbol_info.name)
      except _SymbolNotFoundException:
        symbol_not_found_errors.Write(
            'Did not find function in binary. offset: ' + hex(offset))
        success = False
    unknown_symbol_warnings.WriteEnd('no known section for symbol.')
    symbol_not_found_errors.WriteEnd('symbol not found in the binary.')
    if not success:
      return None
    return ordered_sections

  def _SymbolsAtOffset(self, offset):
    if offset in self._offset_to_symbols:
      return self._offset_to_symbols[offset]
    if offset % 2 and (offset - 1) in self._offset_to_symbols:
      # On ARM, odd addresses are used to signal thumb instruction. They are
      # generated by setting the LSB to 1 (see
      # http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471e/Babfjhia.html).
      # TODO(lizeb): Make sure this hack doesn't propagate to other archs.
      return self._offset_to_symbols[offset - 1]
    raise _SymbolNotFoundException(offset)


def _WarnAboutDuplicates(offsets):
  """Warns about duplicate offsets.

  Args:
    offsets: list of offsets to check for duplicates

  Returns:
    True if there are no duplicates, False otherwise.
  """
  seen_offsets = set()
  ok = True
  for offset in offsets:
    if offset not in seen_offsets:
      seen_offsets.add(offset)
    else:
      ok = False
      logging.warning('Duplicate offset: ' + hex(offset))
  return ok


def _ReadReachedOffsets(filename):
  """Reads and returns a list of reached offsets."""
  with open(filename, 'r') as f:
    offsets = [int(x.rstrip('\n')) for x in f.xreadlines()]
  return offsets


def _CreateArgumentParser():
  parser = argparse.ArgumentParser(
      description='Creates an orderfile from profiles.')
  parser.add_argument('--target-arch', help='Unused')
  parser.add_argument('--reached-offsets', type=str, required=True,
                      help='Path to the reached offsets')
  parser.add_argument('--native-library', type=str, required=True,
                      help='Path to the unstripped instrumented library')
  parser.add_argument('--output', type=str, required=True,
                      help='Output filename')
  return parser


def main():
  parser = _CreateArgumentParser()
  args = parser.parse_args()

  offsets = _ReadReachedOffsets(args.reached_offsets)
  assert offsets
  _WarnAboutDuplicates(offsets)

  processor = process_profiles.SymbolOffsetProcessor(args.native_library)
  ordered_symbols = processor.GetOrderedSymbols(offsets)
  if ordered_symbols is None:
    return 1

  success = False
  temp_filename = None
  output_file = None
  try:
    (fd, temp_filename) = tempfile.mkstemp(dir=os.path.dirname(args.output))
    output_file = os.fdopen(fd, 'w')
    output_file.write('\n'.join(ordered_symbols))
    output_file.close()
    os.rename(temp_filename, args.output)
    temp_filename = None
    success = True
  finally:
    if output_file:
      output_file.close()
    if temp_filename:
      os.remove(temp_filename)

  return 0 if success else 1


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
