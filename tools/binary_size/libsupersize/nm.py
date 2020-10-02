# Copyright 2017 The Chromium Authors. All rights reserved.
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
"""

import collections
import subprocess

import demangle
import parallel
import path_util
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


def CollectAliasesByAddress(elf_path, tool_prefix):
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
  args = [path_util.GetNmPath(tool_prefix), '--no-sort', '--defined-only',
          elf_path]
  # pylint: disable=unexpected-keyword-arg
  proc = subprocess.Popen(
      args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8')
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
  names_by_address = demangle.DemangleSetsInDicts(names_by_address, tool_prefix)

  # Since this is run in a separate process, minimize data passing by returning
  # only aliased symbols.
  # Also: Sort to ensure stable ordering.
  return {
      addr: sorted(names, key=lambda n: (n.startswith('**'), n))
      for addr, names in names_by_address.items()
      if len(names) > 1 or num_outlined_functions_at_address.get(addr, 0) > 1
  }



def _CollectAliasesByAddressAsyncHelper(elf_path, tool_prefix):
  result = CollectAliasesByAddress(elf_path, tool_prefix)
  return parallel.EncodeDictOfLists(result, key_transform=str)


def CollectAliasesByAddressAsync(elf_path, tool_prefix):
  """Calls CollectAliasesByAddress in a helper process. Returns a Result."""
  def decode(encoded):
    return parallel.DecodeDictOfLists(encoded, key_transform=int)

  return parallel.ForkAndCall(
      _CollectAliasesByAddressAsyncHelper, (elf_path, tool_prefix),
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
def RunNmOnIntermediates(target, tool_prefix, output_directory):
  """Returns encoded_symbol_names_by_path, encoded_string_addresses_by_path.

  Args:
    target: Either a single path to a .a (as a string), or a list of .o paths.
  """
  is_archive = isinstance(target, str)
  args = [path_util.GetNmPath(tool_prefix), '--no-sort', '--defined-only']
  if is_archive:
    args.append(target)
  else:
    args.extend(target)
  # pylint: disable=unexpected-keyword-arg
  proc = subprocess.Popen(
      args,
      cwd=output_directory,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      encoding='utf-8')
  # llvm-nm can print 'no symbols' to stderr. Capture and count the number of
  # lines, to be returned to the caller.
  stdout, stderr = proc.communicate()
  assert proc.returncode == 0
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
