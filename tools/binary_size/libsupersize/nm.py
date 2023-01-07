#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs nm on specified .a and .o file, plus some analysis.

CollectAliasesByAddress():
  Runs nm on the elf to collect all symbol names. This reveals symbol names of
  identical-code-folded functions.

CollectAliasesByAddressAsync():
  Runs CollectAliasesByAddress in a subprocess and returns a promise.

RunNmOnIntermediates():
  BulkForkAndCall() target: Runs nm on a .a file or a list of .o files, parses
  the output, extracts symbol information, and (if available) extracts string
  offset information.

CreateUniqueSymbols():
  Creates Symbol objects from nm output.
"""

import argparse
import collections
import logging
import os
import subprocess

import demangle
import models
import parallel
import path_util
import readelf
import sys


def _IsRelevantNmName(name):
  # Skip lines like:
  # 00000000 t $t
  # 00000000 r $d.23
  # 00000344 N
  return name and not name.startswith('$')


def _IsRelevantObjectFileName(name):
  # Prevent marking compiler-generated symbols as candidates for shared paths.
  # E.g., multiple files might have "CSWTCH.12", but they are different symbols.
  #
  # Find these via:
  #   size_info.symbols.GroupedByFullName(min_count=-2).Filter(
  #       lambda s: s.WhereObjectPathMatches('{')).SortedByCount()
  # and then search for {shared}.
  # List of names this applies to:
  #   startup
  #   __tcf_0  <-- Generated for global destructors.
  #   ._79
  #   .Lswitch.table, .Lswitch.table.12
  #   CSWTCH.12
  #   lock.12
  #   table.12
  #   __compound_literal.12
  #   .L.ref.tmp.1
  #   .L.str, .L.str.3
  #   .L__func__.main:  (when using __func__)
  #   .L__FUNCTION__._ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv
  #   .L__PRETTY_FUNCTION__._Unwind_Resume
  #   .L_ZZ24ScaleARGBFilterCols_NEONE9dx_offset  (an array literal)
  if name in ('__tcf_0', 'startup'):
    return False
  if name.startswith('._') and name[2:].isdigit():
    return False
  if name.startswith('.L') and name.find('.', 2) != -1:
    return False

  dot_idx = name.find('.')
  if dot_idx == -1:
    return True
  name = name[:dot_idx]

  return name not in ('CSWTCH', 'lock', '__compound_literal', 'table')


def CollectAliasesByAddress(elf_path):
  """Runs nm on |elf_path| and returns a dict of address->[names]"""
  # Constructors often show up twice, so use sets to ensure no duplicates.
  names_by_address = collections.defaultdict(set)

  # Many OUTLINED_FUNCTION_* entries can coexist on a single address, possibly
  # mixed with regular symbols. However, naively keeping these is bad because:
  # * OUTLINED_FUNCTION_* can have many duplicates. Keeping them would cause
  #   false associations downstream, when looking up object_paths from names.
  # * For addresses with multiple OUTLINED_FUNCTION_* entries, we can't get the
  #   associated object_path (exception: the one entry in the .map file, for LLD
  #   without ThinLTO). So keeping copies around is rather useless.
  # Our solution is to merge OUTLINED_FUNCTION_* entries at the same address
  # into a single symbol. We'd also like to keep track of the number of copies
  # (although it will not be used to compute PSS computation). This is done by
  # writing the count in the name, e.g., '** outlined function * 5'.
  num_outlined_functions_at_address = collections.Counter()

  # About 60mb of output, but piping takes ~30s, and loading it into RAM
  # directly takes 3s.
  args = [path_util.GetNmPath(), '--no-sort', '--defined-only', elf_path]
  # pylint: disable=unexpected-keyword-arg
  proc = subprocess.Popen(args,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL,
                          encoding='utf-8')
  # llvm-nm may write to stderr. Discard to denoise.
  stdout, _ = proc.communicate()
  assert proc.returncode == 0
  for line in stdout.splitlines():
    space_idx = line.find(' ')
    address_str = line[:space_idx]
    section = line[space_idx + 1]
    mangled_name = line[space_idx + 3:]

    # To verify that rodata does not have aliases:
    #   nm --no-sort --defined-only libchrome.so > nm.out
    #   grep -v '\$' nm.out | grep ' r ' | sort | cut -d' ' -f1 > addrs
    #   wc -l < addrs; uniq < addrs | wc -l
    if section not in 'tTW' or not _IsRelevantNmName(mangled_name):
      continue

    address = int(address_str, 16)
    if not address:
      continue
    if mangled_name.startswith('OUTLINED_FUNCTION_'):
      num_outlined_functions_at_address[address] += 1
    else:
      names_by_address[address].add(mangled_name)

  # Need to add before demangling because |names_by_address| changes type.
  for address, count in num_outlined_functions_at_address.items():
    name = '** outlined function' + (' * %d' % count if count > 1 else '')
    names_by_address[address].add(name)

  # Demangle all names.
  demangle.DemangleSetsInDictsInPlace(names_by_address)

  # Since this is run in a separate process, minimize data passing by returning
  # only aliased symbols.
  # Also: Sort to ensure stable ordering.
  return {
      addr: sorted(names, key=lambda n: (n.startswith('**'), n))
      for addr, names in names_by_address.items()
      if len(names) > 1 or num_outlined_functions_at_address.get(addr, 0) > 1
  }


def CreateUniqueSymbols(elf_path, section_ranges):
  """Creates symbols from nm --print-size output.

  Creates only one symbol for each address (does not create symbol aliases).
  """
  # Filter to sections we care about and sort by (address, size).
  section_ranges = [
      x for x in section_ranges.items() if x[0] in models.NATIVE_SECTIONS
  ]
  section_ranges.sort(key=lambda x: x[1])
  min_address = section_ranges[0][1][0]
  max_address = sum(section_ranges[-1][1])

  args = [
      path_util.GetNmPath(), '--no-sort', '--defined-only', '--print-size',
      elf_path
  ]
  # pylint: disable=unexpected-keyword-arg
  stdout = subprocess.check_output(args,
                                   stderr=subprocess.DEVNULL,
                                   encoding='utf-8')
  lines = stdout.splitlines()
  logging.debug('Parsing %d lines of output', len(lines))
  symbols_by_address = {}
  # Example 32-bit output:
  # 00857f94 00000004 t __on_dlclose_late
  # 000001ec r ndk_build_number
  for line in lines:
    tokens = line.split(' ', 3)
    num_tokens = len(tokens)
    if num_tokens < 3:
      # Address with no size and no name.
      continue
    address_str = tokens[0]
    # Check if size is omitted (can happen with binutils but not llvm).
    if num_tokens == 3:
      size_str = '0'
      section = tokens[1]
      mangled_name = tokens[2]
    else:
      size_str = tokens[1]
      section = tokens[2]
      mangled_name = tokens[3]

    if section not in 'BbDdTtRrWw' or not _IsRelevantNmName(mangled_name):
      continue

    address = int(address_str, 16)

    # Ignore symbols outside of sections that we care about.
    # Symbols can still exist in sections that we do not care about if those
    # sections are interleaved. We discard such symbols in the next loop.
    if not min_address <= address < max_address:
      continue

    # Pick the alias that defines a size.
    existing_alias = symbols_by_address.get(address)
    if existing_alias and existing_alias.size > 0:
      continue

    size = int(size_str, 16)

    # E.g.: .str.2.llvm.12282370934750212
    if mangled_name.startswith('.str.'):
      mangled_name = models.STRING_LITERAL_NAME
    elif mangled_name.startswith('__ARMV7PILongThunk_'):
      # Convert thunks from prefix to suffix so that name is demangleable.
      mangled_name = mangled_name[len('__ARMV7PILongThunk_'):] + '.LongThunk'
    elif mangled_name.startswith('__ThumbV7PILongThunk_'):
      mangled_name = mangled_name[len('__ThumbV7PILongThunk_'):] + '.LongThunk'

    # Use address (next loop) to determine between .data and .data.rel.ro.
    section_name = None
    if section in 'Tt':
      section_name = models.SECTION_TEXT
    elif section in 'Rr':
      section_name = models.SECTION_RODATA
    elif section in 'Bb':
      section_name = models.SECTION_BSS

    # No need to demangle names since they will be demangled by
    # DemangleRemainingSymbols().
    symbols_by_address[address] = models.Symbol(section_name,
                                                size,
                                                address=address,
                                                full_name=mangled_name)

  logging.debug('Sorting %d NM symbols', len(symbols_by_address))
  # Sort symbols by address.
  sorted_symbols = sorted(symbols_by_address.values(), key=lambda s: s.address)

  # Assign section to symbols based on address, and size where unspecified.
  # Use address rather than nm's section character to distinguish between
  # .data.rel.ro and .data.
  logging.debug('Assigning section_name and filling in missing sizes')
  section_range_iter = iter(section_ranges)
  section_end = -1
  raw_symbols = []
  active_assembly_sym = None
  for i, sym in enumerate(sorted_symbols):
    # Move to next section if applicable.
    while sym.address >= section_end:
      section_range = next(section_range_iter)
      section_name, (section_start, section_size) = section_range
      section_end = section_start + section_size

    # Skip symbols that don't fall into a section that we care about
    # (e.g. GCC_except_table533 from .eh_frame).
    if sym.address < section_start:
      continue

    if sym.section_name and sym.section_name != section_name:
      logging.warning('Re-assigning section for %r to %s', sym, section_name)
    sym.section_name = section_name

    if i + 1 < len(sorted_symbols):
      next_addr = min(section_end, sorted_symbols[i + 1].address)
    else:
      next_addr = section_end

    # Heuristic: Discard subsequent assembly symbols (no size) that are ALL_CAPS
    # or .-prefixed, since they are likely labels within a function.
    if (active_assembly_sym and sym.size == 0
        and sym.section_name == models.SECTION_TEXT):
      if sym.full_name.startswith('.') or sym.full_name.isupper():
        active_assembly_sym.size += next_addr - sym.address
        # Triggers ~30 times for all of libchrome.so.
        logging.debug('Discarding assembly label: %s', sym.full_name)
        continue

    active_assembly_sym = sym if sym.size == 0 else None

    # For assembly symbols:
    # Add in a size when absent and guard against size overlapping next symbol.
    if active_assembly_sym or sym.end_address > next_addr:
      sym.size = next_addr - sym.address

    raw_symbols.append(sym)

  return raw_symbols


def _CollectAliasesByAddressAsyncHelper(elf_path):
  result = CollectAliasesByAddress(elf_path)
  return parallel.EncodeDictOfLists(result, key_transform=str)


def CollectAliasesByAddressAsync(elf_path):
  """Calls CollectAliasesByAddress in a helper process. Returns a Result."""
  def decode(encoded):
    return parallel.DecodeDictOfLists(encoded, key_transform=int)

  return parallel.ForkAndCall(_CollectAliasesByAddressAsyncHelper, (elf_path, ),
                              decode_func=decode)


def _ParseOneObjectFileNmOutput(lines):
  # Constructors are often repeated because they have the same unmangled
  # name, but multiple mangled names. See:
  # https://stackoverflow.com/questions/6921295/dual-emission-of-constructor-symbols
  symbol_names = set()
  string_addresses = []
  for line in lines:
    if not line:
      break
    space_idx = line.find(' ')  # Skip over address.
    section = line[space_idx + 1]
    mangled_name = line[space_idx + 3:]
    if _IsRelevantNmName(mangled_name):
      # Refer to _IsRelevantObjectFileName() for examples of names.
      if section == 'r' and (
          mangled_name.startswith('.L.str') or
          mangled_name.startswith('.L__') and mangled_name.find('.', 3) != -1):
        # Leave as a string for easier marshalling.
        string_addresses.append(line[:space_idx].lstrip('0') or '0')
      elif _IsRelevantObjectFileName(mangled_name):
        symbol_names.add(mangled_name)
  return symbol_names, string_addresses


# This is a target for BulkForkAndCall().
def RunNmOnIntermediates(target, output_directory):
  """Returns encoded_symbol_names_by_path, encoded_string_addresses_by_path.

  Args:
    target: Either a single path to a .a (as a string), or a list of .o paths.
  """
  is_archive = isinstance(target, str)
  args = [path_util.GetNmPath(), '--no-sort', '--defined-only']
  if is_archive:
    args.append(target)
  else:
    args.extend(target)
  proc = subprocess.Popen(
      args,
      cwd=output_directory,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      encoding='utf-8')
  # llvm-nm can print 'no symbols' to stderr. Capture and count the number of
  # lines, to be returned to the caller.
  stdout, stderr = proc.communicate()
  assert proc.returncode == 0, 'NM failed: ' + ' '.join(args)
  num_no_symbols = len(stderr.splitlines())
  lines = stdout.splitlines()
  # Empty .a file has no output.
  if not lines:
    return parallel.EMPTY_ENCODED_DICT, parallel.EMPTY_ENCODED_DICT
  is_multi_file = not lines[0]
  lines = iter(lines)
  if is_multi_file:
    next(lines)
    path = next(lines)[:-1]  # Path ends with a colon.
  else:
    assert not is_archive
    path = target[0]

  symbol_names_by_path = {}
  string_addresses_by_path = {}
  while path:
    if is_archive:
      # E.g. foo/bar.a(baz.o)
      path = '%s(%s)' % (target, path)

    mangled_symbol_names, string_addresses = _ParseOneObjectFileNmOutput(lines)
    symbol_names_by_path[path] = mangled_symbol_names
    if string_addresses:
      string_addresses_by_path[path] = string_addresses
    path = next(lines, ':')[:-1]

  # The multiprocess API uses pickle, which is ridiculously slow. More than 2x
  # faster to use join & split.
  # TODO(agrieve): We could use path indices as keys rather than paths to cut
  #     down on marshalling overhead.
  return (parallel.EncodeDictOfLists(symbol_names_by_path),
          parallel.EncodeDictOfLists(string_addresses_by_path), num_no_symbols)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory', required=True)
  parser.add_argument('elf_path', type=os.path.realpath)

  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  # Other functions in this file have test entrypoints in object_analyzer.py.
  section_ranges = readelf.SectionInfoFromElf(args.elf_path)
  symbols = CreateUniqueSymbols(args.elf_path, section_ranges)
  for s in symbols:
    print(s)
  logging.warning('Printed %d symbols', len(symbols))


if __name__ == '__main__':
  main()
