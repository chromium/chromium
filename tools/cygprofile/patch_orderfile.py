#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Patch an orderfile.

Starting with a list of symbols in a binary and an orderfile (ordered list of
symbols), matches the symbols in the orderfile and augments each symbol with
the symbols residing at the same address (due to having identical code).  The
output is a list of symbols appropriate for the linker
option --symbol-ordering-file for lld. Note this is not usable with gold (which
uses section names to order the binary).

Note: It is possible to have.
- Several symbols mapping to the same offset in the binary.
- Several offsets for a given symbol (because we strip the ".clone." and other
  suffixes)

The general pipeline is:
1. Get the symbol infos (name, offset, size, section) from the binary
2. Get the symbol names from the orderfile
3. Find the orderfile symbol names in the symbols coming from the binary
4. For each symbol found, get all the symbols at the same address
5. Output them to an updated orderfile suitable lld
"""

import argparse
import collections
import logging
import re
import sys

import symbol_extractor

# Suffixes for symbols.  These are due to method splitting for inlining and
# method cloning for various reasons including constant propagation and
# inter-procedural optimization.
_SUFFIXES = ('.clone.', '.part.', '.isra.', '.constprop.')


# The pattern and format for a linker-generated outlined function.
_OUTLINED_FUNCTION_RE = re.compile(r'OUTLINED_FUNCTION_(?P<index>\d+)$')
_OUTLINED_FUNCTION_FORMAT = 'OUTLINED_FUNCTION_{}'

def RemoveSuffixes(name):
  """Strips method name suffixes from cloning and splitting.

  .clone. comes from cloning in -O3.
  .part.  comes from partial method splitting for inlining.
  .isra.  comes from inter-procedural optimizations.
  .constprop. is cloning for constant propagation.
  """
  for suffix in _SUFFIXES:
    name = name.split(suffix)[0]
  return name


def _UniqueGenerator(generator):
  """Converts a generator to skip yielding elements already seen.

  Example:
    @_UniqueGenerator
    def Foo():
      yield 1
      yield 2
      yield 1
      yield 3

    Foo() yields 1,2,3.
  """
  def _FilteringFunction(*args, **kwargs):
    returned = set()
    for item in generator(*args, **kwargs):
      if item in returned:
        continue
      returned.add(item)
      yield item

  return _FilteringFunction


def _GroupSymbolsByOffset(binary_filename):
  """Produce a map symbol name -> all symbol names at same offset.

  Suffixes are stripped.
  """
  symbol_infos = [
      s._replace(name=RemoveSuffixes(s.name))
      for s in symbol_extractor.SymbolInfosFromBinary(binary_filename)]
  offset_map = symbol_extractor.GroupSymbolInfosByOffset(symbol_infos)
  missing_offsets = 0
  sym_to_matching = {}
  for sym in symbol_infos:
    if sym.offset not in offset_map:
      missing_offsets += 1
      continue
    matching = [s.name for s in offset_map[sym.offset]]
    assert sym.name in matching
    sym_to_matching[sym.name] = matching
  return sym_to_matching


def _GetMaxOutlinedIndex(sym_dict):
  """Find the largest index of an outlined functions.

  See _OUTLINED_FUNCTION_RE for the definition of the index. In practice the
  maximum index equals the total number of outlined functions. This function
  asserts that the index is near the total number of outlined functions.

  Args:
    sym_dict: Dict with symbol names as keys.

  Returns:
    The largest index of an outlined function seen in the keys of |sym_dict|.
  """
  seen = set()
  for sym in sym_dict:
    m = _OUTLINED_FUNCTION_RE.match(sym)
    if m:
      seen.add(int(m.group('index')))
  if not seen:
    return None
  max_index = max(seen)
  # Assert that the number of outlined functions is reasonable compared to the
  # indices we've seen. At the time of writing, outlined functions are indexed
  # consecutively from 0. If this radically changes, then other outlining
  # behavior may have changed to violate some assumptions.
  assert max_index < 2 * len(seen)
  return max_index


def _StripSuffixes(section_list):
  """Remove all suffixes on items in a list of symbols."""
  return [RemoveSuffixes(section) for section in section_list]


def _PatchedSymbols(symbol_to_matching, profiled_symbols, max_outlined_index):
  """Internal computation of an orderfile.

  Args:
    symbol_to_matching: ({symbol name -> [symbols at same offset]}), as from
      _GroupSymbolsByOffset.
    profiled_symbols: ([symbol names]) as from the unpatched orderfile.
    max_outlined_index: (int or None) if not None, add outlined function names
      to the end of the patched orderfile.

  Yields:
    Patched symbols, in a consistent order to profiled_symbols.
  """
  missing_symbol_count = 0
  seen_symbols = set()
  for sym in profiled_symbols:
    if _OUTLINED_FUNCTION_RE.match(sym):
      continue
    if sym in seen_symbols:
      continue
    if sym not in symbol_to_matching:
      missing_symbol_count += 1
      continue
    for matching in symbol_to_matching[sym]:
      if matching in seen_symbols:
        continue
      if _OUTLINED_FUNCTION_RE.match(matching):
        continue
      yield matching
      seen_symbols.add(matching)
    assert sym in seen_symbols
  logging.warning('missing symbol count = %d', missing_symbol_count)

  if max_outlined_index is not None:
    # The number of outlined functions may change with each build, so only
    # ordering the outlined functions currently in the binary will not
    # guarantee ordering after code changes before the next orderfile is
    # generated. So we double the number of outlined functions as a measure of
    # security.
    for idx in range(2 * max_outlined_index + 1):
      yield _OUTLINED_FUNCTION_FORMAT.format(idx)


@_UniqueGenerator
def ReadOrderfile(orderfile):
  """Reads an orderfile and cleans up symbols.

  Args:
    orderfile: The name of the orderfile.

  Yields:
    Symbol names, cleaned and unique.
  """
  with open(orderfile) as f:
    for line in f:
      line = line.strip()
      if line:
        yield line


def GeneratePatchedOrderfile(unpatched_orderfile, native_lib_filename,
                             output_filename, order_outlined=False):
  """Writes a patched orderfile.

  Args:
    unpatched_orderfile: (str) Path to the unpatched orderfile.
    native_lib_filename: (str) Path to the native library.
    output_filename: (str) Path to the patched orderfile.
    order_outlined: (bool) If outlined function symbols are present in the
      native library, then add ordering of them to the orderfile. If there
      are no outlined function symbols present then this flag has no effect.
  """
  symbol_to_matching = _GroupSymbolsByOffset(native_lib_filename)
  if order_outlined:
    max_outlined_index = _GetMaxOutlinedIndex(symbol_to_matching)
    if not max_outlined_index:
      # Only generate ordered outlined functions if they already appeared in
      # the library.
      max_outlined_index = None
  else:
    max_outlined_index = None  # Ignore outlining.
  profiled_symbols = ReadOrderfile(unpatched_orderfile)

  with open(output_filename, 'w') as f:
    # Make sure the anchor functions are located in the right place, here and
    # after everything else.
    # See the comment in //base/android/library_loader/anchor_functions.cc.
    #
    # __cxx_global_var_init is one of the largest symbols (~38kB as of May
    # 2018), called extremely early, and not instrumented.
    for first_section in ('dummy_function_start_of_ordered_text',
                          '__cxx_global_var_init'):
      f.write(first_section + '\n')

    for sym in _PatchedSymbols(symbol_to_matching, profiled_symbols,
                               max_outlined_index):
      f.write(sym + '\n')

    f.write('dummy_function_end_of_ordered_text\n')


def _CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--target-arch', help='Unused')
  parser.add_argument('--unpatched-orderfile', required=True,
                      help='Path to the unpatched orderfile')
  parser.add_argument('--native-library', required=True,
                      help='Path to the native library')
  parser.add_argument('--output-file', required=True, help='Output filename')
  return parser


def main():
  parser = _CreateArgumentParser()
  options = parser.parse_args()
  GeneratePatchedOrderfile(options.unpatched_orderfile, options.native_library,
                           options.output_file)
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(main())
