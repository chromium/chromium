# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main Python API for analyzing binary size."""

import argparse
import calendar
import collections
import dataclasses
import datetime
import functools
import gzip
import itertools
import logging
import os
import posixpath
import re
import shlex
import string
import subprocess
import sys
import tempfile
import time
import zipfile

import apkanalyzer
import ar
import data_quality
import demangle
import describe
import dir_metadata
import dwarfdump
import file_format
import function_signature
import linker_map_parser
import models
import ninja_parser
import nm
import obj_analyzer
import pakfile
import parallel
import path_util
import readelf
import string_extract
import zip_util


# Holds computation state that is live only when an output directory exists.
_OutputDirectoryContext = collections.namedtuple('_OutputDirectoryContext', [
    'elf_object_paths',  # Only when elf_path is also provided.
    'known_inputs',  # Only when elf_path is also provided.
    'output_directory',
    'thin_archives',
])

# When ensuring matching section sizes between .elf and .map files, these
# sections should be ignored. When lld creates a combined library with
# partitions, some sections (like .text) exist in each partition, but the ones
# below are common. At library splitting time, llvm-objcopy pulls what's needed
# from these sections into the new libraries. Hence, the ELF sections will end
# up smaller than the combined .map file sections.
_SECTION_SIZE_BLOCKLIST = ['.symtab', '.shstrtab', '.strtab']


# Tunable constant "knobs" for CreateContainerAndSymbols().
class SectionSizeKnobs:
  def __init__(self):
    # A limit on the number of symbols an address can have, before these symbols
    # are compacted into shared symbols. Increasing this value causes more data
    # to be stored .size files, but is also more expensive.
    # Effect of max_same_name_alias_count (as of Oct 2017, with min_pss = max):
    # 1: shared .text syms = 1772874 bytes, file size = 9.43MiB (645476 syms).
    # 2: shared .text syms = 1065654 bytes, file size = 9.58MiB (669952 syms).
    # 6: shared .text syms = 464058 bytes, file size = 10.11MiB (782693 syms).
    # 10: shared .text syms = 365648 bytes, file size = 10.24MiB (813758 syms).
    # 20: shared .text syms = 86202 bytes, file size = 10.38MiB (854548 syms).
    # 40: shared .text syms = 48424 bytes, file size = 10.50MiB (890396 syms).
    # 50: shared .text syms = 41860 bytes, file size = 10.54MiB (902304 syms).
    # max: shared .text syms = 0 bytes, file size = 11.10MiB (1235449 syms).
    self.max_same_name_alias_count = 40  # 50kb is basically negligable.

    # File name: Source file.
    self.apk_other_files = {
        'assets/icudtl.dat':
        '../../third_party/icu/android/icudtl.dat',
        'assets/snapshot_blob_32.bin':
        '../../v8/snapshot_blob_32.bin',
        'assets/snapshot_blob_64.bin':
        '../../v8/snapshot_blob_64.bin',
        'assets/unwind_cfi_32':
        '../../base/trace_event/cfi_backtrace_android.cc',
        'assets/webapk_dex_version.txt':
        '../../chrome/android/webapk/libs/runtime_library_version.gni',
        'lib/armeabi-v7a/libarcore_sdk_c_minimal.so':
        '../../third_party/arcore-android-sdk/BUILD.gn',
        'lib/armeabi-v7a/libarcore_sdk_c.so':
        '../../third_party/arcore-android-sdk/BUILD.gn',
        'lib/armeabi-v7a/libcrashpad_handler_trampoline.so':
        '../../third_party/crashpad/BUILD.gn',
        'lib/armeabi-v7a/libyoga.so':
        '../../chrome/android/feed/BUILD.gn',
        'lib/armeabi-v7a/libelements.so':
        '../../chrome/android/feed/BUILD.gn',
        'lib/arm64-v8a/libarcore_sdk_c_minimal.so':
        '../../third_party/arcore-android-sdk/BUILD.gn',
        'lib/arm64-v8a/libarcore_sdk_c.so':
        '../../third_party/arcore-android-sdk/BUILD.gn',
        'lib/arm64-v8a/libcrashpad_handler_trampoline.so':
        '../../third_party/crashpad/BUILD.gn',
        'lib/arm64-v8a/libyoga.so':
        '../../chrome/android/feed/BUILD.gn',
        'lib/arm64-v8a/libelements.so':
        '../../chrome/android/feed/BUILD.gn',
    }


@dataclasses.dataclass
class NativeSpec:
  # One (or more) of apk_so_path, map_path, elf_path must be non-None.
  tool_prefix: str  # Never None.
  apk_so_path: str = None
  map_path: str = None
  elf_path: str = None  # Unstripped .so path.
  linker_name: str = None
  track_string_literals: bool = True


@dataclasses.dataclass
class PakSpec:
  # One of pak_paths or apk_pak_paths must be non-None.
  pak_paths: list = None
  apk_pak_paths: list = None
  pak_info_path: str = None


@dataclasses.dataclass
class ApkSpec:
  apk_path: str  # Never None.
  minimal_apks_path: str = None
  mapping_path: str = None
  split_name: str = None
  size_info_prefix: str = None
  analyze_dex: bool = True


def _OpenMaybeGzAsText(path):
  """Calls `gzip.open()` if |path| ends in ".gz", otherwise calls `open()`."""
  if path.endswith('.gz'):
    return gzip.open(path, 'rt')
  return open(path, 'rt')


def _NormalizeNames(raw_symbols):
  """Ensures that all names are formatted in a useful way.

  This includes:
    - Deriving |name| and |template_name| from |full_name|.
    - Stripping of return types (for functions).
    - Moving "vtable for" and the like to be suffixes rather than prefixes.
  """
  found_prefixes = set()
  for symbol in raw_symbols:
    full_name = symbol.full_name

    # See comment in _CalculatePadding() about when this can happen. Don't
    # process names for non-native sections.
    if symbol.IsPak():
      # full_name: "about_ui_resources.grdp: IDR_ABOUT_UI_CREDITS_HTML".
      space_idx = full_name.rindex(' ')
      name = full_name[space_idx + 1:]
      symbol.template_name = name
      symbol.name = name
    elif (full_name.startswith('**') or symbol.IsOverhead()
          or symbol.IsOther()):
      symbol.template_name = full_name
      symbol.name = full_name
    elif symbol.IsDex():
      symbol.full_name, symbol.template_name, symbol.name = (
          function_signature.ParseJava(full_name))
    elif symbol.IsStringLiteral():
      symbol.full_name = full_name
      symbol.template_name = full_name
      symbol.name = full_name
    elif symbol.IsNative():
      # Remove [clone] suffix, and set flag accordingly.
      # Search from left-to-right, as multiple [clone]s can exist.
      # Example name suffixes:
      #     [clone .part.322]  # GCC
      #     [clone .isra.322]  # GCC
      #     [clone .constprop.1064]  # GCC
      #     [clone .11064]  # clang
      # http://unix.stackexchange.com/questions/223013/function-symbol-gets-part-suffix-after-compilation
      idx = full_name.find(' [clone ')
      if idx != -1:
        full_name = full_name[:idx]
        symbol.flags |= models.FLAG_CLONE

      # Clones for C symbols.
      if symbol.section == 't':
        idx = full_name.rfind('.')
        if idx != -1 and full_name[idx + 1:].isdigit():
          new_name = full_name[:idx]
          # Generated symbols that end with .123 but are not clones.
          # Find these via:
          # size_info.symbols.WhereInSection('t').WhereIsGroup().SortedByCount()
          if new_name not in ('__tcf_0', 'startup'):
            full_name = new_name
            symbol.flags |= models.FLAG_CLONE
            # Remove .part / .isra / .constprop.
            idx = full_name.rfind('.', 0, idx)
            if idx != -1:
              full_name = full_name[:idx]

      # E.g.: vtable for FOO
      idx = full_name.find(' for ', 0, 30)
      if idx != -1:
        found_prefixes.add(full_name[:idx + 4])
        full_name = '{} [{}]'.format(full_name[idx + 5:], full_name[:idx])

      # E.g.: virtual thunk to FOO
      idx = full_name.find(' to ', 0, 30)
      if idx != -1:
        found_prefixes.add(full_name[:idx + 3])
        full_name = '{} [{}]'.format(full_name[idx + 4:], full_name[:idx])

      # Strip out return type, and split out name, template_name.
      # Function parsing also applies to non-text symbols.
      # E.g. Function statics.
      symbol.full_name, symbol.template_name, symbol.name = (
          function_signature.Parse(full_name))

      # Remove anonymous namespaces (they just harm clustering).
      symbol.template_name = symbol.template_name.replace(
          '(anonymous namespace)::', '')
      symbol.full_name = symbol.full_name.replace(
          '(anonymous namespace)::', '')
      non_anonymous_name = symbol.name.replace('(anonymous namespace)::', '')
      if symbol.name != non_anonymous_name:
        symbol.flags |= models.FLAG_ANONYMOUS
        symbol.name = non_anonymous_name

    # Allow using "is" to compare names (and should help with RAM). This applies
    # to all symbols.
    function_signature.InternSameNames(symbol)

  logging.debug('Found name prefixes of: %r', found_prefixes)


def _NormalizeObjectPath(path):
  """Normalizes object paths.

  Prefixes are removed: obj/, ../../
  Archive names made more pathy: foo/bar.a(baz.o) -> foo/bar.a/baz.o
  """
  if path.startswith('obj/'):
    # Convert obj/third_party/... -> third_party/...
    path = path[4:]
  elif path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    path = path[6:]
  if path.endswith(')'):
    # Convert foo/bar.a(baz.o) -> foo/bar.a/baz.o so that hierarchical
    # breakdowns consider the .o part to be a separate node.
    start_idx = path.rindex('(')
    path = os.path.join(path[:start_idx], path[start_idx + 1:-1])
  return path


def _NormalizeSourcePath(path):
  """Returns (is_generated, normalized_path)"""
  if path.startswith('gen/'):
    # Convert gen/third_party/... -> third_party/...
    return True, path[4:]
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return False, path[6:]
  return True, path


def _AddSourcePathsUsingObjectPaths(ninja_source_mapper, raw_symbols):
  logging.info('Looking up source paths from ninja files')
  for symbol in raw_symbols:
    if symbol.IsDex() or symbol.IsOther():
      continue
    # Native symbols and pak symbols use object paths.
    object_path = symbol.object_path
    if not object_path:
      continue

    # We don't have source info for prebuilt .a files.
    if not os.path.isabs(object_path) and not object_path.startswith('..'):
      symbol.source_path = ninja_source_mapper.FindSourceForPath(object_path)
  assert ninja_source_mapper.unmatched_paths_count == 0, (
      'One or more source file paths could not be found. Likely caused by '
      '.ninja files being generated at a different time than the .map file.')


def _AddSourcePathsUsingAddress(dwarf_source_mapper, raw_symbols):
  logging.info('Looking up source paths from dwarfdump')
  for symbol in raw_symbols:
    if symbol.section_name != models.SECTION_TEXT:
      continue
    source_path = dwarf_source_mapper.FindSourceForTextAddress(symbol.address)
    if source_path and not os.path.isabs(source_path):
      symbol.source_path = source_path
  # Majority of unmatched queries are for assembly source files (ex libav1d)
  # and v8 builtins.
  assert dwarf_source_mapper.unmatched_queries_ratio < 0.1, (
      'Percentage of failing |dwarf_source_mapper| queries ' +
      '({}%) >= 10% '.format(dwarf_source_mapper.unmatched_queries_ratio * 100)
      + 'FindSourceForTextAddress() likely has a bug.')


def _NormalizePaths(raw_symbols):
  """Fills in the |source_path| attribute and normalizes |object_path|."""
  logging.info('Normalizing source and object paths')
  for symbol in raw_symbols:
    if symbol.object_path:
      symbol.object_path = _NormalizeObjectPath(symbol.object_path)
    if symbol.source_path:
      symbol.generated_source, symbol.source_path = _NormalizeSourcePath(
          symbol.source_path)


def _ComputeAncestorPath(path_list, symbol_count):
  """Returns the common ancestor of the given paths."""
  if not path_list:
    return ''

  prefix = os.path.commonprefix(path_list)
  # Check if all paths were the same.
  if prefix == path_list[0]:
    return prefix

  # Put in buckets to cut down on the number of unique paths.
  if symbol_count >= 100:
    symbol_count_str = '100+'
  elif symbol_count >= 50:
    symbol_count_str = '50-99'
  elif symbol_count >= 20:
    symbol_count_str = '20-49'
  elif symbol_count >= 10:
    symbol_count_str = '10-19'
  else:
    symbol_count_str = str(symbol_count)

  # Put the path count as a subdirectory so that grouping by path will show
  # "{shared}" as a bucket, and the symbol counts as leafs.
  if not prefix:
    return os.path.join('{shared}', symbol_count_str)
  return os.path.join(os.path.dirname(prefix), '{shared}', symbol_count_str)


def _CompactLargeAliasesIntoSharedSymbols(raw_symbols, knobs):
  """Converts symbols with large number of aliases into single symbols.

  The merged symbol's path fields are changed to common-ancestor paths in
  the form: common/dir/{shared}/$SYMBOL_COUNT

  Assumes aliases differ only by path (not by name).
  """
  num_raw_symbols = len(raw_symbols)
  num_shared_symbols = 0
  src_cursor = 0
  dst_cursor = 0
  while src_cursor < num_raw_symbols:
    symbol = raw_symbols[src_cursor]
    raw_symbols[dst_cursor] = symbol
    dst_cursor += 1
    aliases = symbol.aliases
    if aliases and len(aliases) > knobs.max_same_name_alias_count:
      symbol.source_path = _ComputeAncestorPath(
          [s.source_path for s in aliases if s.source_path], len(aliases))
      symbol.object_path = _ComputeAncestorPath(
          [s.object_path for s in aliases if s.object_path], len(aliases))
      symbol.generated_source = all(s.generated_source for s in aliases)
      symbol.aliases = None
      num_shared_symbols += 1
      src_cursor += len(aliases)
    else:
      src_cursor += 1
  raw_symbols[dst_cursor:] = []
  num_removed = src_cursor - dst_cursor
  logging.debug('Converted %d aliases into %d shared-path symbols',
                num_removed, num_shared_symbols)


def _ConnectNmAliases(raw_symbols):
  """Ensures |aliases| is set correctly for all symbols."""
  prev_sym = raw_symbols[0]
  for sym in raw_symbols[1:]:
    # Don't merge bss symbols.
    if sym.address > 0 and prev_sym.address == sym.address:
      # Don't merge padding-only symbols (** symbol gaps).
      if prev_sym.size > 0:
        # Don't merge if already merged.
        if prev_sym.aliases is None or prev_sym.aliases is not sym.aliases:
          if prev_sym.aliases:
            prev_sym.aliases.append(sym)
          else:
            prev_sym.aliases = [prev_sym, sym]
          sym.aliases = prev_sym.aliases
    prev_sym = sym


def _AssignNmAliasPathsAndCreatePathAliases(raw_symbols, object_paths_by_name):
  num_found_paths = 0
  num_unknown_names = 0
  num_path_mismatches = 0
  num_aliases_created = 0
  ret = []
  for symbol in raw_symbols:
    ret.append(symbol)
    full_name = symbol.full_name
    # '__typeid_' symbols appear in linker .map only, and not nm output.
    if full_name.startswith('__typeid_'):
      if object_paths_by_name.get(full_name):
        logging.warning('Found unexpected __typeid_ symbol in nm output: %s',
                        full_name)
      continue

    # Don't skip if symbol.IsBss(). This is needed for LLD-LTO to work, since
    # .bss object_path data are unavailable for linker_map_parser, and need to
    # be extracted here. For regular LLD flow, incorrect aliased symbols can
    # arise. But that's a lesser evil compared to having LLD-LTO .bss missing
    # object_path and source_path.
    # TODO(huangs): Fix aliased symbols for the LLD case.
    if (symbol.IsStringLiteral() or
        not full_name or
        full_name[0] in '*.' or  # e.g. ** merge symbols, .Lswitch.table
        full_name == 'startup'):
      continue

    object_paths = object_paths_by_name.get(full_name)
    if object_paths:
      num_found_paths += 1
    else:
      # Happens a lot with code that has LTO enabled (linker creates symbols).
      num_unknown_names += 1
      continue

    if symbol.object_path and symbol.object_path not in object_paths:
      if num_path_mismatches < 10:
        logging.warning('Symbol path reported by .map not found by nm.')
        logging.warning('sym=%r', symbol)
        logging.warning('paths=%r', object_paths)
      object_paths.append(symbol.object_path)
      object_paths.sort()
      num_path_mismatches += 1

    symbol.object_path = object_paths[0]

    if len(object_paths) > 1:
      # Create one symbol for each object_path.
      aliases = symbol.aliases or [symbol]
      symbol.aliases = aliases
      num_aliases_created += len(object_paths) - 1
      for object_path in object_paths[1:]:
        new_sym = models.Symbol(
            symbol.section_name, symbol.size, address=symbol.address,
            full_name=full_name, object_path=object_path, aliases=aliases)
        aliases.append(new_sym)
        ret.append(new_sym)

  logging.debug('Cross-referenced %d symbols with nm output. '
                'num_unknown_names=%d num_path_mismatches=%d '
                'num_aliases_created=%d',
                num_found_paths, num_unknown_names, num_path_mismatches,
                num_aliases_created)
  # Currently: num_unknown_names=1246 out of 591206 (0.2%).
  if num_unknown_names > len(raw_symbols) * 0.01:
    logging.warning('Abnormal number of symbols not found in .o files (%d)',
                    num_unknown_names)
  return ret


def _DiscoverMissedObjectPaths(raw_symbols, known_inputs):
  # Missing object paths are caused by .a files added by -l flags, which are not
  # listed as explicit inputs within .ninja rules.
  missed_inputs = set()
  for symbol in raw_symbols:
    path = symbol.object_path
    if path.endswith(')'):
      # Convert foo/bar.a(baz.o) -> foo/bar.a
      path = path[:path.rindex('(')]
    if path and path not in known_inputs:
      missed_inputs.add(path)
  return missed_inputs


def _CreateMergeStringsReplacements(merge_string_syms,
                                    list_of_positions_by_object_path):
  """Creates replacement symbols for |merge_syms|."""
  ret = []
  STRING_LITERAL_NAME = models.STRING_LITERAL_NAME
  assert len(merge_string_syms) == len(list_of_positions_by_object_path)
  tups = zip(merge_string_syms, list_of_positions_by_object_path)
  for merge_sym, positions_by_object_path in tups:
    merge_sym_address = merge_sym.address
    new_symbols = []
    ret.append(new_symbols)
    for object_path, positions in positions_by_object_path.items():
      for offset, size in positions:
        address = merge_sym_address + offset
        symbol = models.Symbol(
            models.SECTION_RODATA,
            size,
            address=address,
            full_name=STRING_LITERAL_NAME,
            object_path=object_path)
        new_symbols.append(symbol)

  logging.debug('Created %d string literal symbols', sum(len(x) for x in ret))
  logging.debug('Sorting string literals')
  for symbols in ret:
    # For de-duping & alias creation, order by address & size.
    # For alias symbol ordering, sort by object_path.
    symbols.sort(key=lambda x: (x.address, -x.size, x.object_path))

  logging.debug('Deduping string literals')
  num_removed = 0
  size_removed = 0
  num_aliases = 0
  for i, symbols in enumerate(ret):
    if not symbols:
      continue
    prev_symbol = symbols[0]
    new_symbols = [prev_symbol]
    for symbol in symbols[1:]:
      padding = symbol.address - prev_symbol.end_address
      if (prev_symbol.address == symbol.address and
          prev_symbol.size == symbol.size):
        # String is an alias.
        num_aliases += 1
        aliases = prev_symbol.aliases
        if aliases:
          aliases.append(symbol)
          symbol.aliases = aliases
        else:
          aliases = [prev_symbol, symbol]
          prev_symbol.aliases = aliases
          symbol.aliases = aliases
      elif padding + symbol.size <= 0:
        # String is a substring of prior one.
        num_removed += 1
        size_removed += symbol.size
        continue
      elif padding < 0:
        # String overlaps previous one. Adjust to not overlap.
        symbol.address -= padding
        symbol.size += padding
      new_symbols.append(symbol)
      prev_symbol = symbol
    ret[i] = new_symbols

  logging.debug(
      'Removed %d overlapping string literals (%d bytes) & created %d aliases',
                num_removed, size_removed, num_aliases)
  return ret


def _UpdateSymbolNamesFromNm(raw_symbols, names_by_address):
  """Updates raw_symbols names with extra information from nm."""
  logging.debug('Update symbol names')
  # linker_map_parser extracts '** outlined function' without knowing how many
  # such symbols exist at each address. nm has this information, and stores the
  # value as, e.g., '** outlined function * 5'. Copy the information over.
  for s in raw_symbols:
    if s.full_name.startswith('** outlined function'):
      name_list = names_by_address.get(s.address)
      if name_list:
        for name in name_list:
          if name.startswith('** outlined function'):
            s.full_name = name
            break


def _AddNmAliases(raw_symbols, names_by_address):
  """Adds symbols that were removed by identical code folding."""
  # Step 1: Create list of (index_of_symbol, name_list).
  logging.debug('Creating alias list')
  replacements = []
  num_new_symbols = 0
  num_missing = 0
  missing_names = collections.defaultdict(list)
  for i, s in enumerate(raw_symbols):
    # Don't alias padding-only symbols (e.g. ** symbol gap)
    if s.size_without_padding == 0:
      continue
    # Also skip artificial symbols that won't appear in nm output.
    if s.full_name.startswith('** CFI jump table'):
      continue
    name_list = names_by_address.get(s.address)
    if name_list:
      if s.full_name not in name_list:
        num_missing += 1
        missing_names[s.full_name].append(s.address)
        # Sometimes happens for symbols from assembly files.
        if num_missing < 10:
          logging.debug('Name missing from aliases: %s %s (addr=%x)',
                        s.full_name, name_list, s.address)
        continue
      replacements.append((i, name_list))
      num_new_symbols += len(name_list) - 1

  if missing_names and logging.getLogger().isEnabledFor(logging.INFO):
    for address, names in names_by_address.items():
      for name in names:
        if name in missing_names:
          logging.info('Missing name %s is at address %x instead of [%s]' %
              (name, address, ','.join('%x' % a for a in missing_names[name])))

  if float(num_new_symbols) / len(raw_symbols) < .05:
    logging.warning('Number of aliases is oddly low (%.0f%%). It should '
                    'usually be around 25%%. Ensure --tool-prefix is correct. ',
                    float(num_new_symbols) / len(raw_symbols) * 100)

  # Step 2: Create new symbols as siblings to each existing one.
  logging.debug('Creating %d new symbols from nm output', num_new_symbols)
  expected_num_symbols = len(raw_symbols) + num_new_symbols
  ret = []
  prev_src = 0
  for cur_src, name_list in replacements:
    ret += raw_symbols[prev_src:cur_src]
    prev_src = cur_src + 1
    sym = raw_symbols[cur_src]
    # Create symbols (|sym| gets recreated and discarded).
    new_syms = []
    for full_name in name_list:
      # Do not set |aliases| in order to avoid being pruned by
      # _CompactLargeAliasesIntoSharedSymbols(), which assumes aliases differ
      # only by path. The field will be set afterwards by _ConnectNmAliases().
      new_syms.append(models.Symbol(
          sym.section_name, sym.size, address=sym.address, full_name=full_name))
    ret += new_syms
  ret += raw_symbols[prev_src:]
  assert expected_num_symbols == len(ret)
  return ret


def LoadAndPostProcessSizeInfo(path, file_obj=None):
  """Returns a SizeInfo for the given |path|."""
  logging.debug('Loading results from: %s', path)
  size_info = file_format.LoadSizeInfo(path, file_obj=file_obj)
  logging.info('Normalizing symbol names')
  _NormalizeNames(size_info.raw_symbols)
  logging.info('Loaded %d symbols', len(size_info.raw_symbols))
  return size_info


def LoadAndPostProcessDeltaSizeInfo(path, file_obj=None):
  """Returns a tuple of SizeInfos for the given |path|."""
  logging.debug('Loading results from: %s', path)
  before_size_info, after_size_info = file_format.LoadDeltaSizeInfo(
      path, file_obj=file_obj)
  logging.info('Normalizing symbol names')
  _NormalizeNames(before_size_info.raw_symbols)
  _NormalizeNames(after_size_info.raw_symbols)
  logging.info('Loaded %d + %d symbols', len(before_size_info.raw_symbols),
               len(after_size_info.raw_symbols))
  return before_size_info, after_size_info


def _ListSplits(minimal_apks_path):
  ret = []
  with zipfile.ZipFile(minimal_apks_path) as z:
    for filename in z.namelist():
      # E.g.:
      # splits/base-master.apk
      # splits/base-en.apk
      # splits/vr-master.apk
      # splits/vr-en.apk
      m = re.match(r'splits/(.*)-master\.apk', filename)
      if m:
        ret.append(m.group(1))
  # Make "base" comes first since that's the main chunk of work.
  # Also so that --abi-filter detection looks at it first.
  return sorted(ret, key=lambda x: (x != 'base', x))


def _ExtendSectionRange(section_range_by_name, section_name, delta_size):
  (prev_address, prev_size) = section_range_by_name.get(section_name, (0, 0))
  section_range_by_name[section_name] = (prev_address, prev_size + delta_size)


def CreateMetadata(*, build_config, apk_spec, native_spec, source_directory,
                   output_directory):
  """Creates metadata dict while updating |build_config|.

  Returns:
    A dict of models.METADATA_* -> values. Performs "best effort" extraction
    using available data.
  """
  logging.debug('Constructing metadata')

  def update_build_config(key, value):
    if key in build_config:
      old_value = build_config[key]
      if value != old_value:
        raise ValueError('Inconsistent {}: {} (was {})'.format(
            key, value, old_value))
    else:
      build_config[key] = value

  metadata = {}

  # Ensure all paths are relative to output directory to make them hermetic.
  if output_directory:
    shorten_path = lambda path: os.path.relpath(path, output_directory)
    gn_args = _ParseGnArgs(os.path.join(output_directory, 'args.gn'))
    update_build_config(models.BUILD_CONFIG_GN_ARGS, gn_args)
  else:
    # If output directory is unavailable, just store basenames.
    shorten_path = os.path.basename

  # Deduce GIT revision (cached via @lru_cache).
  git_rev = _DetectGitRevision(source_directory)
  if git_rev:
    update_build_config(models.BUILD_CONFIG_GIT_REVISION, git_rev)

  if native_spec:
    relative_tool_prefix = path_util.ToToolsSrcRootRelative(
        native_spec.tool_prefix)
    update_build_config(models.BUILD_CONFIG_TOOL_PREFIX, relative_tool_prefix)

    if native_spec.map_path:
      metadata[models.METADATA_ELF_ALGORITHM] = 'linker_map'
    elif native_spec.elf_path:
      metadata[models.METADATA_ELF_ALGORITHM] = 'dwarf'
    else:
      metadata[models.METADATA_ELF_ALGORITHM] = 'sections'

    if native_spec.linker_name:
      update_build_config(models.BUILD_CONFIG_LINKER_NAME,
                          native_spec.linker_name)

    if native_spec.elf_path:
      metadata[models.METADATA_ELF_FILENAME] = shorten_path(
          native_spec.elf_path)
      architecture = readelf.ArchFromElf(native_spec.elf_path,
                                         native_spec.tool_prefix)
      # TODO(agrieve): We could add these in when elf_path=None and apk_so_path.
      metadata[models.METADATA_ELF_ARCHITECTURE] = architecture
      timestamp_obj = datetime.datetime.utcfromtimestamp(
          os.path.getmtime(native_spec.elf_path))
      timestamp = calendar.timegm(timestamp_obj.timetuple())
      metadata[models.METADATA_ELF_MTIME] = timestamp
      build_id = readelf.BuildIdFromElf(native_spec.elf_path,
                                        native_spec.tool_prefix)
      metadata[models.METADATA_ELF_BUILD_ID] = build_id

      relocations_count = _CountRelocationsFromElf(native_spec.elf_path,
                                                   native_spec.tool_prefix)
      metadata[models.METADATA_ELF_RELOCATIONS_COUNT] = relocations_count

    if native_spec.map_path:
      metadata[models.METADATA_MAP_FILENAME] = shorten_path(
          native_spec.map_path)

  if apk_spec:
    metadata[models.METADATA_APK_SIZE] = os.path.getsize(apk_spec.apk_path)
    if apk_spec.minimal_apks_path:
      metadata[models.METADATA_APK_FILENAME] = shorten_path(
          apk_spec.minimal_apks_path)
      metadata[models.METADATA_APK_SPLIT_NAME] = apk_spec.split_name
    else:
      metadata[models.METADATA_APK_FILENAME] = shorten_path(apk_spec.apk_path)

  logging.debug('Constructing metadata (done)')
  return metadata


def _ResolveThinArchivePaths(raw_symbols, thin_archives):
  """Converts object_paths for thin archives to external .o paths."""
  for symbol in raw_symbols:
    object_path = symbol.object_path
    if object_path.endswith(')'):
      start_idx = object_path.rindex('(')
      archive_path = object_path[:start_idx]
      if archive_path in thin_archives:
        subpath = object_path[start_idx + 1:-1]
        symbol.object_path = ar.CreateThinObjectPath(archive_path, subpath)


def _DeduceObjectPathForSwitchTables(raw_symbols, object_paths_by_name):
  strip_num_suffix_regexp = re.compile(r'\s+\(\.\d+\)$')
  num_switch_tables = 0
  num_unassigned = 0
  num_deduced = 0
  num_arbitrations = 0
  for s in raw_symbols:
    if s.full_name.startswith('Switch table for '):
      num_switch_tables += 1
      # Strip 'Switch table for ' prefix.
      name = s.full_name[17:]
      # Strip, e.g., ' (.123)' suffix.
      name = re.sub(strip_num_suffix_regexp, '', name)
      object_paths = object_paths_by_name.get(name, None)
      if not s.object_path:
        if object_paths is None:
          num_unassigned += 1
        else:
          num_deduced += 1
          # If ambiguity arises, arbitrate by taking the first.
          s.object_path = object_paths[0]
          if len(object_paths) > 1:
            num_arbitrations += 1
      else:
        assert object_paths and s.object_path in object_paths
  if num_switch_tables > 0:
    logging.info(
        'Found %d switch tables: Deduced %d object paths with ' +
        '%d arbitrations. %d remain unassigned.', num_switch_tables,
        num_deduced, num_arbitrations, num_unassigned)


def _NameStringLiterals(raw_symbols, elf_path, tool_prefix):
  # Assign ASCII-readable string literals names like "string contents".
  STRING_LENGTH_CUTOFF = 30

  PRINTABLE_TBL = [False] * 256
  for ch in string.printable:
    PRINTABLE_TBL[ord(ch)] = True

  for sym, name in string_extract.ReadStringLiterals(raw_symbols, elf_path,
                                                     tool_prefix):
    # Newlines and tabs are used as delimiters in file_format.py
    # At this point, names still have a terminating null byte.
    name = name.replace(b'\n', b'').replace(b'\t', b'').strip(b'\00')
    is_printable = all(PRINTABLE_TBL[c] for c in name)
    if is_printable:
      name = name.decode('ascii')
      if len(name) > STRING_LENGTH_CUTOFF:
        sym.full_name = '"{}[...]"'.format(name[:STRING_LENGTH_CUTOFF])
      else:
        sym.full_name = '"{}"'.format(name)
    else:
      sym.full_name = models.STRING_LITERAL_NAME


def _ParseElfInfo(native_spec, outdir_context=None):
  """Adds ELF section ranges and symbols."""
  assert native_spec.map_path or native_spec.elf_path, (
      'Need a linker map or an ELF file.')
  assert native_spec.map_path or not native_spec.track_string_literals, (
      'track_string_literals not yet implemented without map file')
  if native_spec.elf_path:
    elf_section_ranges = readelf.SectionInfoFromElf(native_spec.elf_path,
                                                    native_spec.tool_prefix)

    # Run nm on the elf file to retrieve the list of symbol names per-address.
    # This list is required because the .map file contains only a single name
    # for each address, yet multiple symbols are often coalesced when they are
    # identical. This coalescing happens mainly for small symbols and for C++
    # templates. Such symbols make up ~500kb of libchrome.so on Android.
    elf_nm_result = nm.CollectAliasesByAddressAsync(native_spec.elf_path,
                                                    native_spec.tool_prefix)

    # Run nm on all .o/.a files to retrieve the symbol names within them.
    # The list is used to detect when mutiple .o files contain the same symbol
    # (e.g. inline functions), and to update the object_path / source_path
    # fields accordingly.
    # Looking in object files is required because the .map file choses a
    # single path for these symbols.
    # Rather than record all paths for each symbol, set the paths to be the
    # common ancestor of all paths.
    if outdir_context and native_spec.map_path:
      bulk_analyzer = obj_analyzer.BulkObjectFileAnalyzer(
          native_spec.tool_prefix,
          outdir_context.output_directory,
          track_string_literals=native_spec.track_string_literals)
      bulk_analyzer.AnalyzePaths(outdir_context.elf_object_paths)

  if native_spec.map_path:
    logging.info('Parsing Linker Map')
    with _OpenMaybeGzAsText(native_spec.map_path) as f:
      map_section_ranges, raw_symbols, linker_map_extras = (
          linker_map_parser.MapFileParser().Parse(native_spec.linker_name, f))

      if outdir_context and outdir_context.thin_archives:
        _ResolveThinArchivePaths(raw_symbols, outdir_context.thin_archives)
  else:
    logging.info('Collecting symbols from nm')
    raw_symbols = nm.CreateUniqueSymbols(native_spec.elf_path,
                                         native_spec.tool_prefix,
                                         elf_section_ranges)

  if native_spec.elf_path and native_spec.map_path:
    logging.debug('Validating section sizes')
    differing_elf_section_sizes = {}
    differing_map_section_sizes = {}
    for k, (_, elf_size) in elf_section_ranges.items():
      if k in _SECTION_SIZE_BLOCKLIST:
        continue
      (_, map_size) = map_section_ranges.get(k)
      if map_size != elf_size:
        differing_map_section_sizes[k] = map_size
        differing_elf_section_sizes[k] = elf_size
    if differing_map_section_sizes:
      logging.error('ELF file and .map file do not agree on section sizes.')
      logging.error('readelf: %r', differing_elf_section_sizes)
      logging.error('.map file: %r', differing_map_section_sizes)
      sys.exit(1)

  if native_spec.elf_path and native_spec.map_path and outdir_context:
    missed_object_paths = _DiscoverMissedObjectPaths(
        raw_symbols, outdir_context.known_inputs)
    missed_object_paths = ar.ExpandThinArchives(
        missed_object_paths, outdir_context.output_directory)[0]
    bulk_analyzer.AnalyzePaths(missed_object_paths)
    bulk_analyzer.SortPaths()
    if native_spec.track_string_literals:
      merge_string_syms = [s for s in raw_symbols if
                           s.full_name == '** merge strings' or
                           s.full_name == '** lld merge strings']
      # More likely for there to be a bug in supersize than an ELF to not have a
      # single string literal.
      assert merge_string_syms
      string_ranges = [(s.address, s.size) for s in merge_string_syms]
      bulk_analyzer.AnalyzeStringLiterals(native_spec.elf_path, string_ranges)

  # Map file for some reason doesn't demangle all names.
  # Demangle prints its own log statement.
  demangle.DemangleRemainingSymbols(raw_symbols, native_spec.tool_prefix)

  object_paths_by_name = {}
  if native_spec.elf_path:
    logging.info(
        'Adding symbols removed by identical code folding (as reported by nm)')
    # This normally does not block (it's finished by this time).
    names_by_address = elf_nm_result.get()
    _UpdateSymbolNamesFromNm(raw_symbols, names_by_address)

    raw_symbols = _AddNmAliases(raw_symbols, names_by_address)

    if native_spec.map_path and outdir_context:
      object_paths_by_name = bulk_analyzer.GetSymbolNames()
      logging.debug(
          'Fetched path information for %d symbols from %d files',
          len(object_paths_by_name),
          len(outdir_context.elf_object_paths) + len(missed_object_paths))
      _DeduceObjectPathForSwitchTables(raw_symbols, object_paths_by_name)
      # For aliases, this provides path information where there wasn't any.
      logging.info('Creating aliases for symbols shared by multiple paths')
      raw_symbols = _AssignNmAliasPathsAndCreatePathAliases(
          raw_symbols, object_paths_by_name)

      if native_spec.track_string_literals:
        logging.info('Waiting for string literal extraction to complete.')
        list_of_positions_by_object_path = bulk_analyzer.GetStringPositions()
      bulk_analyzer.Close()

      if native_spec.track_string_literals:
        logging.info('Deconstructing ** merge strings into literals')
        replacements = _CreateMergeStringsReplacements(merge_string_syms,
            list_of_positions_by_object_path)
        for merge_sym, literal_syms in zip(merge_string_syms, replacements):
          # Don't replace if no literals were found.
          if literal_syms:
            # Re-find the symbols since aliases cause their indices to change.
            idx = raw_symbols.index(merge_sym)
            # This assignment is a bit slow (causes array to be shifted), but
            # is fast enough since len(merge_string_syms) < 10.
            raw_symbols[idx:idx + 1] = literal_syms

  if native_spec.map_path:
    linker_map_parser.DeduceObjectPathsFromThinMap(raw_symbols,
                                                   linker_map_extras)

  if native_spec.elf_path and native_spec.track_string_literals:
    _NameStringLiterals(raw_symbols, native_spec.elf_path,
                        native_spec.tool_prefix)

  # If we have an ELF file, use its ranges as the source of truth, since some
  # sections can differ from the .map.
  return (elf_section_ranges if native_spec.elf_path else map_section_ranges,
          raw_symbols, object_paths_by_name)


class _ResourceSourceMapper:
  def __init__(self, size_info_prefix, knobs):
    self._knobs = knobs
    self._res_info = self._LoadResInfo(size_info_prefix)
    self._pattern_dollar_underscore = re.compile(r'\$+(.*?)(?:__\d)+')
    self._pattern_version_suffix = re.compile(r'-v\d+/')

  @staticmethod
  def _ParseResInfoFile(res_info_path):
    with open(res_info_path, 'r') as info_file:
      return dict(l.rstrip().split('\t') for l in info_file)

  def _LoadResInfo(self, size_info_prefix):
    apk_res_info_path = size_info_prefix + '.res.info'
    res_info_without_root = self._ParseResInfoFile(apk_res_info_path)
    # We package resources in the res/ folder only in the apk.
    res_info = {
        os.path.join('res', dest): source
        for dest, source in res_info_without_root.items()
    }
    res_info.update(self._knobs.apk_other_files)
    return res_info

  def FindSourceForPath(self, path):
    # Sometimes android adds $ in front and __# before extension.
    path = self._pattern_dollar_underscore.sub(r'\1', path)
    ret = self._res_info.get(path)
    if ret:
      return ret
    # Android build tools may append extra -v flags for the root dir.
    path = self._pattern_version_suffix.sub('/', path)
    ret = self._res_info.get(path)
    if ret:
      return ret
    return None


class _ResourcePathDeobfuscator:

  def __init__(self, pathmap_path):
    self._pathmap = self._LoadResourcesPathmap(pathmap_path)

  def _LoadResourcesPathmap(self, pathmap_path):
    """Load the pathmap of obfuscated resource paths.

    Returns: A dict mapping from obfuscated paths to original paths or an
             empty dict if passed a None |pathmap_path|.
    """
    if pathmap_path is None:
      return {}

    pathmap = {}
    with open(pathmap_path, 'r') as f:
      for line in f:
        line = line.strip()
        if line.startswith('--') or line == '':
          continue
        original, renamed = line.split(' -> ')
        pathmap[renamed] = original
    return pathmap

  def MaybeRemapPath(self, path):
    long_path = self._pathmap.get(path)
    if long_path:
      return long_path
    # if processing a .minimal.apks, we are actually just processing the base
    # split.
    long_path = self._pathmap.get('base/{}'.format(path))
    if long_path:
      # The first 5 chars are 'base/', which we don't need because we are
      # looking directly inside the base apk.
      return long_path[5:]
    return path


def _ParseApkOtherSymbols(*, apk_spec, native_spec, section_ranges,
                          resources_pathmap_path, metadata, knobs):
  apk_so_path = native_spec and native_spec.apk_so_path
  res_source_mapper = _ResourceSourceMapper(apk_spec.size_info_prefix, knobs)
  resource_deobfuscator = _ResourcePathDeobfuscator(resources_pathmap_path)
  apk_symbols = []
  dex_size = 0
  zip_info_total = 0
  zipalign_total = 0
  with zipfile.ZipFile(apk_spec.apk_path) as z:
    signing_block_size = zip_util.MeasureApkSignatureBlock(z)
    for zip_info in z.infolist():
      zip_info_total += zip_info.compress_size
      # Account for zipalign overhead that exists in local file header.
      zipalign_total += zip_util.ReadZipInfoExtraFieldLength(z, zip_info)
      # Account for zipalign overhead that exists in central directory header.
      # Happens when python aligns entries in apkbuilder.py, but does not
      # exist when using Android's zipalign. E.g. for bundle .apks files.
      zipalign_total += len(zip_info.extra)
      # Skip files that we explicitly analyze: .so, .dex, and .pak.
      if zip_info.filename == apk_so_path:
        continue
      if apk_spec.analyze_dex and zip_info.filename.endswith('.dex'):
        dex_size += zip_info.file_size
        continue
      if zip_info.filename.endswith('.pak'):
        continue

      resource_filename = resource_deobfuscator.MaybeRemapPath(
          zip_info.filename)
      source_path = res_source_mapper.FindSourceForPath(resource_filename)
      if source_path is None:
        source_path = os.path.join(models.APK_PREFIX_PATH, resource_filename)
      apk_symbols.append(
          models.Symbol(
              models.SECTION_OTHER,
              zip_info.compress_size,
              source_path=source_path,
              full_name=resource_filename))  # Full name must disambiguate

  # Store zipalign overhead and signing block size as metadata rather than an
  # "Overhead:" symbol because they fluctuate in size, and would be a source of
  # noise in symbol diffs if included as symbols (http://crbug.com/1130754).
  # Might be even better if we had an option in Tiger Viewer to ignore certain
  # symbols, but taking this as a short-cut for now.
  metadata[models.METADATA_ZIPALIGN_OVERHEAD] = zipalign_total
  metadata[models.METADATA_SIGNING_BLOCK_SIZE] = signing_block_size

  # Overhead includes:
  #  * Size of all local zip headers (minus zipalign padding).
  #  * Size of central directory & end of central directory.
  overhead_size = (os.path.getsize(apk_spec.apk_path) - zip_info_total -
                   zipalign_total - signing_block_size)
  assert overhead_size >= 0, 'Apk overhead must be non-negative'
  zip_overhead_symbol = models.Symbol(
      models.SECTION_OTHER, overhead_size, full_name='Overhead: APK file')
  apk_symbols.append(zip_overhead_symbol)
  _ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                      sum(s.size for s in apk_symbols))
  return dex_size, apk_symbols


def _CalculateElfOverhead(section_ranges, elf_path):
  if elf_path:
    section_sizes_total_without_bss = sum(
        size for k, (address, size) in section_ranges.items()
        if k not in models.BSS_SECTIONS)
    elf_overhead_size = (
        os.path.getsize(elf_path) - section_sizes_total_without_bss)
    assert elf_overhead_size >= 0, (
        'Negative ELF overhead {}'.format(elf_overhead_size))
    return elf_overhead_size
  return 0


def _AddUnattributedSectionSymbols(raw_symbols, section_ranges):
  # Create symbols for ELF sections not covered by existing symbols.
  logging.info('Searching for symbol gaps...')
  new_syms_by_section = collections.defaultdict(list)
  seen_sections = set()

  for section_name, group in itertools.groupby(
      raw_symbols, lambda s: s.section_name):
    seen_sections.add(section_name)
    # Get last Last symbol in group.
    for sym in group:
      pass
    end_address = sym.end_address  # pylint: disable=undefined-loop-variable
    size_from_syms = end_address - section_ranges[section_name][0]
    overhead = section_ranges[section_name][1] - size_from_syms
    assert overhead >= 0, (
        ('End of last symbol (%x) in section %s is %d bytes after the end of '
         'section from readelf (%x).') % (end_address, section_name, -overhead,
                                          sum(section_ranges[section_name])))
    if overhead > 0 and section_name not in models.BSS_SECTIONS:
      new_syms_by_section[section_name].append(
          models.Symbol(section_name,
                        overhead,
                        address=end_address,
                        full_name='** {} (unattributed)'.format(section_name)))
      logging.info('Last symbol in %s does not reach end of section, gap=%d',
                   section_name, overhead)

  # Sections that should not bundle into ".other".
  unsummed_sections, summed_sections = models.ClassifySections(
      section_ranges.keys())
  ret = []
  other_elf_symbols = []
  # Sort keys to ensure consistent order (> 1 sections may have address = 0).
  for section_name, (_, section_size) in list(section_ranges.items()):
    if section_name in seen_sections:
      continue
    # Handle sections that don't appear in |raw_symbols|.
    if (section_name not in unsummed_sections
        and section_name not in summed_sections):
      other_elf_symbols.append(
          models.Symbol(models.SECTION_OTHER,
                        section_size,
                        full_name='** ELF Section: {}'.format(section_name)))
      _ExtendSectionRange(section_ranges, models.SECTION_OTHER, section_size)
    else:
      ret.append(
          models.Symbol(section_name,
                        section_size,
                        full_name='** ELF Section: {}'.format(section_name)))
  other_elf_symbols.sort(key=lambda s: (s.address, s.full_name))

  # TODO(agrieve): It would probably simplify things to use a dict of
  #     section_name->raw_symbols while creating symbols.
  # Merge |new_syms_by_section| into |raw_symbols| while maintaining ordering.
  for section_name, group in itertools.groupby(
      raw_symbols, lambda s: s.section_name):
    ret.extend(group)
    ret.extend(new_syms_by_section[section_name])
  return ret, other_elf_symbols


def _ParseNinjaFiles(output_directory, elf_path=None):
  linker_elf_path = elf_path
  if elf_path:
    # For partitioned libraries, the actual link command outputs __combined.so.
    partitioned_elf_path = elf_path.replace('.so', '__combined.so')
    if os.path.exists(partitioned_elf_path):
      linker_elf_path = partitioned_elf_path

  logging.info('Parsing ninja files, looking for %s.',
               (linker_elf_path or 'source mapping only (elf_path=None)'))

  source_mapper, ninja_elf_object_paths = ninja_parser.Parse(
      output_directory, linker_elf_path)

  logging.debug('Parsed %d .ninja files. Linker inputs=%d',
                source_mapper.parsed_file_count,
                len(ninja_elf_object_paths or []))
  if elf_path:
    assert ninja_elf_object_paths, (
        'Failed to find link command in ninja files for ' +
        os.path.relpath(linker_elf_path, output_directory))

  return source_mapper, ninja_elf_object_paths


def CreateContainerAndSymbols(*,
                              knobs,
                              container_name,
                              metadata,
                              apk_spec,
                              pak_spec,
                              native_spec,
                              source_directory,
                              output_directory=None,
                              resources_pathmap_path=None,
                              pak_id_map=None):
  """Creates a Container (with sections sizes) and symbols for a SizeInfo.

  Args:
    knobs: Instance of SectionSizeKnobs.
    container_name: Name for the created Container. May be '' if only one
        Container exists.
    metadata: Metadata dict from CreateMetadata().
    apk_spec: Instance of ApkSpec, or None.
    pak_spec: Instance of PakSpec, or None.
    native_spec: Instance of NativeSpec, or None.
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    source_directory: Path to source root.
    resources_pathmap_path: Path to the pathmap file that maps original
        resource paths to shortened resource paths.
    pak_id_map: Instance of PakIdMap, or None.

  Returns:
    A tuple of (container, raw_symbols).
    containers is a Container instance that stores metadata and section_sizes
    (section_sizes maps section names to respective sizes).
    raw_symbols is a list of Symbol objects.
  """
  knobs = knobs or SectionSizeKnobs()
  apk_elf_result = None
  if apk_spec and native_spec and native_spec.apk_so_path:
    # Extraction takes around 1 second, so do it in parallel.
    apk_elf_result = parallel.ForkAndCall(
        _ElfInfoFromApk,
        (apk_spec.apk_path, native_spec.apk_so_path, native_spec.tool_prefix))

  ninja_source_mapper = None
  dwarf_source_mapper = None
  section_ranges = {}
  raw_symbols = []
  object_paths_by_name = None
  if native_spec:
    ninja_elf_object_paths = None
    if output_directory and native_spec.map_path:
      # Finds all objects passed to the linker and creates a map of .o -> .cc.
      ninja_source_mapper, ninja_elf_object_paths = _ParseNinjaFiles(
          output_directory, native_spec.elf_path)
    elif native_spec.elf_path:
      logging.info('Parsing source path info via dwarfdump')
      dwarf_source_mapper = dwarfdump.CreateAddressSourceMapper(
          native_spec.elf_path, native_spec.tool_prefix)
      logging.info('Found %d source paths across %s ranges',
                   dwarf_source_mapper.NumberOfPaths(),
                   dwarf_source_mapper.num_ranges)

    # Start by finding elf_object_paths so that nm can run on them while the
    # linker .map is being parsed.
    if ninja_elf_object_paths:
      elf_object_paths, thin_archives = ar.ExpandThinArchives(
          ninja_elf_object_paths, output_directory)
      known_inputs = set(elf_object_paths)
      known_inputs.update(ninja_elf_object_paths)
    else:
      elf_object_paths = []
      known_inputs = None
      # When we don't know which elf file is used, just search all paths.
      # TODO(agrieve): Seems to be used only for tests. Remove?
      if ninja_source_mapper:
        thin_archives = set(
            p for p in ninja_source_mapper.IterAllPaths() if p.endswith('.a')
            and ar.IsThinArchive(os.path.join(output_directory, p)))
      else:
        thin_archives = None

    outdir_context = None
    if output_directory:
      outdir_context = _OutputDirectoryContext(
          elf_object_paths=elf_object_paths,
          known_inputs=known_inputs,
          output_directory=output_directory,
          thin_archives=thin_archives)

    if native_spec.elf_path or native_spec.map_path:
      section_ranges, raw_symbols, object_paths_by_name = _ParseElfInfo(
          native_spec, outdir_context=outdir_context)

      if pak_id_map and native_spec.map_path:
        # For trichrome, pak files are in different apks than native library,
        # so need to pass along pak_id_map separately and ensure
        # TrichromeLibrary appears first in .ssargs file.
        logging.debug('Extracting pak IDs from symbol names')
        pak_id_map.Update(object_paths_by_name, ninja_source_mapper)

  if apk_elf_result:
    logging.debug('Extracting section sizes from .so within .apk')
    apk_build_id, section_ranges, elf_overhead_size = apk_elf_result.get()
    if metadata and models.METADATA_ELF_BUILD_ID in metadata:
      assert apk_build_id == metadata[models.METADATA_ELF_BUILD_ID], (
          'BuildID from apk_elf_result did not match')
  elif native_spec and native_spec.elf_path:
    # Strip ELF before capturing section information to avoid recording
    # debug sections.
    with tempfile.NamedTemporaryFile(
        suffix=os.path.basename(native_spec.elf_path)) as f:
      strip_path = path_util.GetStripPath(native_spec.tool_prefix)
      subprocess.run([strip_path, '-o', f.name, native_spec.elf_path],
                     check=True)
      section_ranges = readelf.SectionInfoFromElf(f.name,
                                                  native_spec.tool_prefix)
      elf_overhead_size = _CalculateElfOverhead(section_ranges, f.name)

  if native_spec:
    raw_symbols, other_elf_symbols = _AddUnattributedSectionSymbols(
        raw_symbols, section_ranges)

  other_symbols = []
  if apk_spec and apk_spec.size_info_prefix:
    # Can modify |section_ranges|.
    dex_size, other_symbols = _ParseApkOtherSymbols(
        apk_spec=apk_spec,
        native_spec=native_spec,
        section_ranges=section_ranges,
        resources_pathmap_path=resources_pathmap_path,
        metadata=metadata,
        knobs=knobs)

    if apk_spec.analyze_dex:
      logging.info('Analyzing Dex')
      dex_symbols = apkanalyzer.CreateDexSymbols(apk_spec.apk_path,
                                                 apk_spec.mapping_path,
                                                 apk_spec.size_info_prefix)

      # We can't meaningfully track section size of dex methods vs other, so
      # just fake the size of dex methods as the sum of symbols, and make
      # "dex other" responsible for any unattributed bytes.
      dex_method_size = int(
          round(
              sum(s.pss for s in dex_symbols
                  if s.section_name == models.SECTION_DEX_METHOD)))
      section_ranges[models.SECTION_DEX_METHOD] = (0, dex_method_size)
      section_ranges[models.SECTION_DEX] = (0, dex_size - dex_method_size)

      dex_other_size = int(
          round(
              sum(s.pss for s in dex_symbols
                  if s.section_name == models.SECTION_DEX)))
      unattributed_dex = section_ranges[models.SECTION_DEX][1] - dex_other_size
      # Compare against -5 instead of 0 to guard against round-off errors.
      assert unattributed_dex >= -5, ('Dex symbols take up more space than '
                                      'the dex sections have available')
      if unattributed_dex > 0:
        dex_symbols.append(
            models.Symbol(
                models.SECTION_DEX,
                unattributed_dex,
                full_name='** .dex (unattributed - includes string literals)'))
      raw_symbols.extend(dex_symbols)

  if pak_spec:
    logging.debug('Creating Pak symbols')
    if pak_spec.apk_pak_paths:
      assert apk_spec.size_info_prefix
      # Can modify |section_ranges|.
      raw_symbols += pakfile.CreatePakSymbolsFromApk(section_ranges,
                                                     apk_spec.apk_path,
                                                     pak_spec.apk_pak_paths,
                                                     apk_spec.size_info_prefix,
                                                     pak_id_map)
    else:
      # Can modify |section_ranges|.
      raw_symbols += pakfile.CreatePakSymbolsFromFiles(section_ranges,
                                                       pak_spec.pak_paths,
                                                       pak_spec.pak_info_path,
                                                       output_directory,
                                                       pak_id_map)

  if native_spec:
    other_symbols.extend(other_elf_symbols)
    if native_spec.elf_path:
      elf_overhead_symbol = models.Symbol(models.SECTION_OTHER,
                                          elf_overhead_size,
                                          full_name='Overhead: ELF file')
      _ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                          elf_overhead_size)
      other_symbols.append(elf_overhead_symbol)

  # Always have .other come last.
  other_symbols.sort(key=lambda s: (s.IsOverhead(), s.full_name.startswith(
      '**'), s.address, s.full_name))
  raw_symbols.extend(other_symbols)

  if ninja_source_mapper:
    _AddSourcePathsUsingObjectPaths(ninja_source_mapper, raw_symbols)
  elif dwarf_source_mapper:
    _AddSourcePathsUsingAddress(dwarf_source_mapper, raw_symbols)
  _NormalizePaths(raw_symbols)

  dir_metadata.PopulateComponents(raw_symbols, source_directory)
  logging.info('Converting excessive aliases into shared-path symbols')
  _CompactLargeAliasesIntoSharedSymbols(raw_symbols, knobs)

  if native_spec:
    logging.debug('Connecting nm aliases')
    _ConnectNmAliases(raw_symbols)

  section_sizes = {k: size for k, (address, size) in section_ranges.items()}
  container = models.Container(name=container_name,
                               metadata=metadata,
                               section_sizes=section_sizes)
  for symbol in raw_symbols:
    symbol.container = container

  file_format.SortSymbols(raw_symbols, check_already_mostly_sorted=True)

  return container, raw_symbols


def CreateSizeInfo(build_config,
                   container_list,
                   raw_symbols_list,
                   normalize_names=True):
  """Performs operations on all symbols and creates a SizeInfo object."""
  assert len(container_list) == len(raw_symbols_list)

  all_raw_symbols = []
  for raw_symbols in raw_symbols_list:
    file_format.CalculatePadding(raw_symbols)

    # Do not call _NormalizeNames() during archive since that method tends to
    # need tweaks over time. Calling it only when loading .size files allows for
    # more flexibility.
    if normalize_names:
      _NormalizeNames(raw_symbols)

    all_raw_symbols += raw_symbols

  return models.SizeInfo(build_config, container_list, all_raw_symbols)


@functools.lru_cache
def _DetectGitRevision(directory):
  """Runs git rev-parse to get the SHA1 hash of the current revision.

  Args:
    directory: Path to directory where rev-parse command will be run.

  Returns:
    A string with the SHA1 hash, or None if an error occured.
  """
  try:
    git_rev = subprocess.check_output(
        ['git', '-C', directory, 'rev-parse', 'HEAD']).decode('ascii')
    return git_rev.rstrip()
  except Exception:
    logging.warning('Failed to detect git revision for file metadata.')
    return None


def _ElfIsMainPartition(elf_path, tool_prefix):
  section_ranges = readelf.SectionInfoFromElf(elf_path, tool_prefix)
  return models.SECTION_PART_END in section_ranges.keys()


def _CountRelocationsFromElf(elf_path, tool_prefix):
  args = [path_util.GetObjDumpPath(tool_prefix), '--private-headers', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  relocations = re.search('REL[AR]?COUNT\s*(.+)', stdout).group(1)
  return int(relocations, 16)


@functools.lru_cache
def _ParseGnArgs(args_path):
  """Returns a list of normalized "key=value" strings."""
  args = {}
  with open(args_path) as f:
    for l in f:
      # Strips #s even if within string literal. Not a problem in practice.
      parts = l.split('#')[0].split('=')
      if len(parts) != 2:
        continue
      args[parts[0].strip()] = parts[1].strip()
  return ["%s=%s" % x for x in sorted(args.items())]


def _DetectLinkerName(map_path):
  with _OpenMaybeGzAsText(map_path) as f:
    return linker_map_parser.DetectLinkerNameFromMapFile(f)


def _ElfInfoFromApk(apk_path, apk_so_path, tool_prefix):
  """Returns a tuple of (build_id, section_ranges, elf_overhead_size)."""
  with zip_util.UnzipToTemp(apk_path, apk_so_path) as temp:
    build_id = readelf.BuildIdFromElf(temp, tool_prefix)
    section_ranges = readelf.SectionInfoFromElf(temp, tool_prefix)
    elf_overhead_size = _CalculateElfOverhead(section_ranges, temp)
    return build_id, section_ranges, elf_overhead_size


def _AddContainerArguments(parser, is_top_args=False):
  """Add arguments applicable to a single container."""

  # Main file argument: Exactly one should be specified (perhaps via -f).
  # _IdentifyInputFile() should be kept updated.
  group = parser.add_argument_group(title='Main Input')
  group = group.add_mutually_exclusive_group(required=True)
  group.add_argument('-f',
                     metavar='FILE',
                     help='Auto-identify input file type.')
  group.add_argument('--apk-file',
                     help='.apk file to measure. Other flags can generally be '
                     'derived when this is used.')
  group.add_argument('--minimal-apks-file',
                     help='.minimal.apks file to measure. Other flags can '
                     'generally be derived when this is used.')
  group.add_argument('--elf-file', help='Path to input ELF file.')
  group.add_argument('--map-file',
                     help='Path to input .map(.gz) file. Defaults to '
                     '{{elf_file}}.map(.gz)?. If given without '
                     '--elf-file, no size metadata will be recorded.')
  group.add_argument('--pak-file',
                     action='append',
                     default=[],
                     dest='pak_files',
                     help='Paths to pak files.')
  if is_top_args:
    group.add_argument('--ssargs-file',
                       help='Path to SuperSize multi-container arguments file.')

  group = parser.add_argument_group(title='What to Analyze')
  group.add_argument('--java-only',
                     action='store_true',
                     help='Run on only Java symbols')
  group.add_argument('--native-only',
                     action='store_true',
                     help='Run on only native symbols')
  group.add_argument('--no-java',
                     action='store_true',
                     help='Do not run on Java symbols')
  group.add_argument('--no-native',
                     action='store_true',
                     help='Do not run on native symbols')

  group = parser.add_argument_group(title='Analysis Options for Native Code')
  if is_top_args:
    group.add_argument(
        '--tool-prefix',
        help='Path prefix for binaries such as nm, readelf, objdump')
  group.add_argument('--no-string-literals',
                     dest='track_string_literals',
                     default=True,
                     action='store_false',
                     help='Disable breaking down "** merge strings" into more '
                     'granular symbols.')
  group.add_argument('--no-map-file',
                     dest='ignore_linker_map',
                     action='store_true',
                     help='Use debug information to capture symbol sizes '
                     'instead of linker map file.')
  # Used by tests to override path to APK-discovered files.
  group.add_argument('--aux-elf-file', help=argparse.SUPPRESS)
  group.add_argument(
      '--aux-map-file',
      help='Path to linker map to use when --elf-file is provided')

  group = parser.add_argument_group(title='APK options')
  group.add_argument('--mapping-file',
                     help='Proguard .mapping file for deobfuscation.')
  group.add_argument('--resources-pathmap-file',
                     help='.pathmap.txt file that contains a maping from '
                     'original resource paths to shortened resource paths.')
  group.add_argument('--abi-filter',
                     dest='abi_filters',
                     action='append',
                     help='For apks with multiple ABIs, break down native '
                     'libraries for this ABI. Defaults to 64-bit when both '
                     '32 and 64 bit are present.')

  group = parser.add_argument_group(title='Analysis Options for Pak Files')
  group.add_argument('--pak-info-file',
                     help='This file should contain all ids found in the pak '
                     'files that have been passed in. If not specified, '
                     '${pak_file}.info is assumed.')

  group = parser.add_argument_group(title='Analysis Options (shared)')
  group.add_argument('--source-directory',
                     help='Custom path to the root source directory.')
  group.add_argument('--output-directory',
                     help='Path to the root build directory.')
  if is_top_args:
    group.add_argument('--no-output-directory',
                       action='store_true',
                       help='Do not auto-detect --output-directory.')
    group.add_argument('--include-padding',
                       action='store_true',
                       help='Include a padding field for each symbol, '
                       'instead of rederiving from consecutive symbols '
                       'on file load.')
    group.add_argument('--check-data-quality',
                       action='store_true',
                       help='Perform sanity checks to ensure there is no '
                       'missing data.')


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  _AddContainerArguments(parser, is_top_args=True)


def _IdentifyInputFile(args, on_config_error):
  """Identifies main input file type from |args.f|, and updates |args|.

  Identification is performed on filename alone, i.e., the file need not exist.
  The result is written to a field in |args|. If the field exists then it
  simply gets overwritten.

  If '.' is missing from |args.f| then --elf-file is assumed.

  Returns:
    The primary input file.
"""
  if args.f:
    if args.f.endswith('.minimal.apks'):
      args.minimal_apks_file = args.f
    elif args.f.endswith('.apk'):
      args.apk_file = args.f
    elif args.f.endswith('.so') or '.' not in os.path.basename(args.f):
      args.elf_file = args.f
    elif args.f.endswith('.map') or args.f.endswith('.map.gz'):
      args.map_file = args.f
    elif args.f.endswith('.pak'):
      args.pak_files.append(args.f)
    elif args.f.endswith('.ssargs'):
      # Fails if trying to nest them, which should never happen.
      args.ssargs_file = args.f
    else:
      on_config_error('Cannot identify file ' + args.f)
    args.f = None

  ret = [
      args.apk_file, args.elf_file, args.minimal_apks_file,
      args.__dict__.get('ssargs_file'), args.map_file
  ] + (args.pak_files or [])
  ret = [v for v in ret if v]
  if not ret:
    on_config_error(
        'Must pass at least one of --apk-file, --minimal-apks-file, '
        '--elf-file, --map-file, --pak-file, --ssargs-file')
  return ret[0]


def ParseSsargs(lines):
  """Parses .ssargs data.

  An .ssargs file is a text file to specify multiple containers as input to
  SuperSize-archive. After '#'-based comments, start / end whitespaces, and
  empty lines are stripped, each line specifies a distinct container. Format:
  * Positional argument: |name| for the container.
  * Main input file specified by -f, --apk-file, --elf-file, etc.:
    * Can be an absolute path.
    * Can be a relative path. In this case, it's up to the caller to supply the
      base directory.
    * -f switch must not specify another .ssargs file.
  * For supported switches: See _AddContainerArguments().

  Args:
    lines: An iterator containing lines of .ssargs data.
  Returns:
    A list of arguments, one for each container.
  Raises:
    ValueError: Parse error, including input line number.
  """
  sub_args_list = []
  parser = argparse.ArgumentParser(add_help=False)
  parser.error = lambda msg: (_ for _ in ()).throw(ValueError(msg))
  parser.add_argument('name')
  _AddContainerArguments(parser)
  try:
    for lineno, line in enumerate(lines, 1):
      toks = shlex.split(line, comments=True)
      if not toks:  # Skip if line is empty after stripping comments.
        continue
      sub_args_list.append(parser.parse_args(toks))
  except ValueError as e:
    e.args = ('Line %d: %s' % (lineno, e.args[0]), )
    raise e
  return sub_args_list


def _UpdateLinkerNameAndToolPrefix(tentative_output_dir, native_spec):
  if not native_spec.map_path:
    return native_spec
  native_spec.linker_name = _DetectLinkerName(native_spec.map_path)
  logging.info('Linker name: %s', native_spec.linker_name)

  tool_prefix_finder = path_util.ToolPrefixFinder(
      value=native_spec.tool_prefix,
      output_directory=tentative_output_dir,
      linker_name=native_spec.linker_name)
  native_spec.tool_prefix = tool_prefix_finder.Finalized()
  return native_spec


def _CreateNativeSpecs(*, tentative_output_dir, apk_path, elf_path, map_path,
                       abi_filters, track_string_literals, ignore_linker_map,
                       tool_prefix, on_config_error):
  if (map_path and not map_path.endswith('.map')
      and not map_path.endswith('.map.gz')):
    on_config_error('Expected --map-file to end with .map or .map.gz')

  apk_so_path = None
  if apk_path:
    with zipfile.ZipFile(apk_path) as z:
      lib_infos = [
          f for f in z.infolist()
          if f.filename.endswith('.so') and f.file_size > 0
      ]
    if abi_filters:
      lib_infos = [
          l for l in lib_infos if any(f in l.filename for f in abi_filters)
      ]
    if lib_infos:
      # TODO(agrieve): Analyze more than just one library.
      apk_so_path = max(lib_infos, key=lambda x: x.file_size).filename

    if apk_so_path:
      if apk_so_path.endswith('_partition.so'):
        # TODO(agrieve): Support symbol breakdowns for partitions (they exist in
        #     the __combined .map file. Debug information (nm output) is shared
        #     with base partition.
        logging.debug('Not breaking down %s: partitioned library', apk_so_path)
        return []

      if not elf_path and tentative_output_dir:
        elf_path = os.path.join(
            tentative_output_dir, 'lib.unstripped',
            posixpath.basename(apk_so_path.replace('crazy.', '')))
        # E.g. libcrashpad_handler_trampoline.so is missing from lib.unstripped.
        if not os.path.exists(elf_path):
          elf_path = None
          logging.debug('Not breaking down %s: missing in lib.unstripped',
                        apk_so_path)
        else:
          logging.debug('Detected --elf-file=%s', elf_path)

  if not tentative_output_dir:
    logging.warning('Cannot break down native symbols without output_dir')

  if ignore_linker_map:
    map_path = None
  elif elf_path and not map_path:
    if _ElfIsMainPartition(elf_path, tool_prefix):
      map_path = elf_path.replace('.so', '__combined.so') + '.map'
    else:
      map_path = elf_path + '.map'
    if not os.path.exists(map_path):
      map_path += '.gz'
      if not os.path.exists(map_path):
        map_path = None

    if map_path:
      logging.debug('Detected --map-file=%s', map_path)

  if not (apk_so_path or map_path or elf_path):
    return []

  # TODO(crbug.com/1193507): Implement string literal tracking without map
  #     files. nm emits some string literal symbols, but most are missing.
  track_string_literals = bool(track_string_literals and map_path)
  native_spec = NativeSpec(tool_prefix=tool_prefix,
                           apk_so_path=apk_so_path,
                           map_path=map_path,
                           elf_path=elf_path,
                           track_string_literals=track_string_literals)
  return [_UpdateLinkerNameAndToolPrefix(tentative_output_dir, native_spec)]


def _DeduceAuxPaths(args, apk_prefix):
  mapping_path = args.mapping_file
  resources_pathmap_path = args.resources_pathmap_file
  if apk_prefix:
    if not mapping_path:
      mapping_path = apk_prefix + '.mapping'
      logging.debug('Detected --mapping-file=%s', mapping_path)
    if not resources_pathmap_path:
      possible_pathmap_path = apk_prefix + '.pathmap.txt'
      # This could be pointing to a stale pathmap file if path shortening was
      # previously enabled but is disabled for the current build. However, since
      # current apk/aab will have unshortened paths, looking those paths up in
      # the stale pathmap which is keyed by shortened paths would not find any
      # mapping and thus should not cause any issues.
      if os.path.exists(possible_pathmap_path):
        resources_pathmap_path = possible_pathmap_path
        logging.debug('Detected --resources-pathmap-file=%s',
                      resources_pathmap_path)
  return mapping_path, resources_pathmap_path


def _ReadMultipleArgsFromStream(lines, base_dir, err_prefix, on_config_error):
  try:
    ret = ParseSsargs(lines)
  except ValueError as e:
    on_config_error('%s: %s' % (err_prefix, e.args[0]))
  for sub_args in ret:
    for k, v in sub_args.__dict__.items():
      # Translate file arguments to be relative to |sub_dir|.
      if (k.endswith('_file') or k == 'f') and isinstance(v, str):
        sub_args.__dict__[k] = os.path.join(base_dir, v)
  return ret


def _ReadMultipleArgsFromFile(ssargs_file, on_config_error):
  with open(ssargs_file, 'r') as fh:
    lines = list(fh)
  err_prefix = 'In file ' + ssargs_file
  # Supply |base_dir| as the directory containing the .ssargs file, to ensure
  # consistent behavior wherever SuperSize-archive runs.
  base_dir = os.path.dirname(os.path.abspath(ssargs_file))
  return _ReadMultipleArgsFromStream(lines, base_dir, err_prefix,
                                     on_config_error)


# Both |top_args| and |sub_args| may be modified.
def _ProcessContainerArgs(top_args,
                          sub_args,
                          container_name,
                          on_config_error,
                          apk_path=None,
                          split_name=None):
  sub_args.source_directory = (sub_args.source_directory
                               or top_args.source_directory)
  sub_args.output_directory = (sub_args.output_directory
                               or top_args.output_directory)
  analyze_native = not (sub_args.java_only or sub_args.no_native
                        or top_args.java_only or top_args.no_native)

  apk_path = apk_path or sub_args.apk_file
  if split_name:
    container_name = '{}/{}.apk'.format(container_name, split_name)
    # Make on-demand a part of the name so that:
    # * It's obvious from the name which DFMs are on-demand.
    # * Diffs that change an on-demand status show as adds/removes.
    if _IsOnDemand(apk_path):
      container_name += '?'

  apk_prefix = sub_args.minimal_apks_file or sub_args.apk_file
  if apk_prefix:
    # Allow either .minimal.apks or just .apks.
    apk_prefix = apk_prefix.replace('.minimal.apks', '.aab')
    apk_prefix = apk_prefix.replace('.apks', '.aab')

  mapping_path, resources_pathmap_path = _DeduceAuxPaths(sub_args, apk_prefix)

  apk_spec = None
  if apk_prefix:
    apk_spec = ApkSpec(apk_path=apk_path,
                       minimal_apks_path=sub_args.minimal_apks_file,
                       mapping_path=mapping_path,
                       split_name=split_name)
    if top_args.output_directory:
      apk_spec.size_info_prefix = os.path.join(top_args.output_directory,
                                               'size-info',
                                               os.path.basename(apk_prefix))
    apk_spec.analyze_dex = not (sub_args.native_only or sub_args.no_java
                                or top_args.native_only or top_args.no_java)

  pak_spec = None
  apk_pak_paths = None
  if apk_spec:
    with zipfile.ZipFile(apk_spec.apk_path) as z:
      apk_pak_paths = [
          f.filename for f in z.infolist() if f.filename.endswith('.pak')
      ]
  if apk_pak_paths or sub_args.pak_files:
    pak_spec = PakSpec(pak_paths=sub_args.pak_files,
                       pak_info_path=sub_args.pak_info_file,
                       apk_pak_paths=apk_pak_paths)

  if analyze_native:
    tool_prefix_finder = path_util.ToolPrefixFinder(
        value=top_args.tool_prefix,
        output_directory=top_args.output_directory,
        linker_name='lld')

    # Allow top-level --abi-filter to override values set in .ssargs.
    abi_filters = top_args.abi_filters or sub_args.abi_filters
    aux_elf_file = sub_args.aux_elf_file
    aux_map_file = sub_args.aux_map_file
    if split_name not in (None, 'base'):
      aux_elf_file = None
      aux_map_file = None

    native_specs = _CreateNativeSpecs(
        tentative_output_dir=top_args.output_directory,
        apk_path=apk_path,
        elf_path=sub_args.elf_file or aux_elf_file,
        map_path=sub_args.map_file or aux_map_file,
        abi_filters=abi_filters,
        track_string_literals=(top_args.track_string_literals
                               and sub_args.track_string_literals),
        ignore_linker_map=(top_args.ignore_linker_map
                           or sub_args.ignore_linker_map),
        tool_prefix=tool_prefix_finder.Finalized(),
        on_config_error=on_config_error)

    # For app bundles, use a consistent ABI for all splits.
    if split_name == 'base' and native_specs and not abi_filters:
      abi = posixpath.basename(posixpath.dirname(native_specs[0].apk_so_path))
      logging.info('Detected --abi-filter %s', abi)
      top_args.abi_filters = [abi]
  else:
    native_specs = []

  logging.info('Container Params: %r', sub_args.__dict__)
  return (sub_args, apk_spec, pak_spec, native_specs, container_name,
          resources_pathmap_path)


def _IsOnDemand(apk_path):
  # Check if the manifest specifies whether or not to extract native libs.
  output = subprocess.check_output([
      path_util.GetAapt2Path(), 'dump', 'xmltree', '--file',
      'AndroidManifest.xml', apk_path
  ]).decode('ascii')

  def parse_attr(name):
    # http://schemas.android.com/apk/res/android:isFeatureSplit(0x0101055b)=true
    # http://schemas.android.com/apk/distribution:onDemand=true
    m = re.search(name + r'(?:\(.*?\))?=(\w+)', output)
    return m and m.group(1) == 'true'

  is_feature_split = parse_attr('android:isFeatureSplit')
  # Can use <dist:on-demand>, or <module dist:onDemand="true">.
  on_demand = parse_attr(
      'distribution:onDemand') or 'distribution:on-demand' in output
  on_demand = bool(on_demand and is_feature_split)

  return on_demand


def _IterSubArgs(top_args, on_config_error):
  """Generates main paths (may be deduced) for each containers given by input.

  Yields:
    For each container, main paths and other info needed to create size_info.
  """
  main_file = _IdentifyInputFile(top_args, on_config_error)
  if top_args.no_output_directory:
    top_args.output_directory = None
  else:
    output_directory_finder = path_util.OutputDirectoryFinder(
        value=top_args.output_directory,
        any_path_within_output_directory=main_file)
    top_args.output_directory = output_directory_finder.Finalized()

  if not top_args.source_directory:
    top_args.source_directory = path_util.GetSrcRootFromOutputDirectory(
        top_args.output_directory)
    assert top_args.source_directory

  if top_args.ssargs_file:
    sub_args_list = _ReadMultipleArgsFromFile(top_args.ssargs_file,
                                              on_config_error)
  else:
    sub_args_list = [top_args]

  # Do a quick first pass to ensure inputs have been built.
  for sub_args in sub_args_list:
    main_file = _IdentifyInputFile(sub_args, on_config_error)
    if not os.path.exists(main_file):
      raise Exception('Input does not exist: ' + main_file)

  # Each element in |sub_args_list| specifies a container.
  for sub_args in sub_args_list:
    main_file = _IdentifyInputFile(sub_args, on_config_error)
    if hasattr(sub_args, 'name'):
      container_name = sub_args.name
    else:
      container_name = os.path.basename(main_file)
    if set(container_name) & set('<>?'):
      parser.error('Container name cannot have characters in "<>?"')


    # If needed, extract .apk file to a temp file and process that instead.
    if sub_args.minimal_apks_file:
      for split_name in _ListSplits(sub_args.minimal_apks_file):
        with zip_util.UnzipToTemp(
            sub_args.minimal_apks_file,
            'splits/{}-master.apk'.format(split_name)) as temp:
          yield _ProcessContainerArgs(top_args,
                                      sub_args,
                                      container_name,
                                      on_config_error,
                                      apk_path=temp,
                                      split_name=split_name)
    else:
      yield _ProcessContainerArgs(top_args, sub_args, container_name,
                                  on_config_error)


def Run(top_args, on_config_error):
  if not top_args.size_file.endswith('.size'):
    on_config_error('size_file must end with .size')
  if top_args.check_data_quality:
    start_time = time.time()

  knobs = SectionSizeKnobs()
  build_config = {}
  seen_container_names = set()
  container_list = []
  raw_symbols_list = []
  pak_id_map = pakfile.PakIdMap()

  # Iterate over each container.
  for (sub_args, apk_spec, pak_spec, native_specs, container_name,
       resources_pathmap_path) in _IterSubArgs(top_args, on_config_error):
    if not native_specs:
      native_specs = [None]
    for native_spec in native_specs:
      if container_name in seen_container_names:
        raise ValueError('Duplicate container name: {}'.format(container_name))
      seen_container_names.add(container_name)
      logging.info('Starting on container %s', container_name)

      metadata = CreateMetadata(build_config=build_config,
                                apk_spec=apk_spec,
                                native_spec=native_spec,
                                source_directory=sub_args.source_directory,
                                output_directory=sub_args.output_directory)
      container, raw_symbols = CreateContainerAndSymbols(
          knobs=knobs,
          container_name=container_name,
          metadata=metadata,
          apk_spec=apk_spec,
          pak_spec=pak_spec,
          native_spec=native_spec,
          source_directory=sub_args.source_directory,
          output_directory=sub_args.output_directory,
          resources_pathmap_path=resources_pathmap_path,
          pak_id_map=pak_id_map)

      container_list.append(container)
      raw_symbols_list.append(raw_symbols)

  size_info = CreateSizeInfo(build_config,
                             container_list,
                             raw_symbols_list,
                             normalize_names=False)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in data_quality.DescribeSizeInfoCoverage(size_info):
      logging.debug(line)
  logging.info('Recorded info for %d symbols', len(size_info.raw_symbols))
  for container in size_info.containers:
    logging.info('Recording metadata: \n  %s',
                 '\n  '.join(describe.DescribeDict(container.metadata)))

  logging.info('Saving result to %s', top_args.size_file)
  file_format.SaveSizeInfo(size_info,
                           top_args.size_file,
                           include_padding=top_args.include_padding)
  size_in_mb = os.path.getsize(top_args.size_file) / 1024.0 / 1024.0
  logging.info('Done. File size is %.2fMiB.', size_in_mb)

  if top_args.check_data_quality:
    logging.info('Checking data quality')
    data_quality.CheckDataQuality(size_info, top_args.track_string_literals)
    duration = (time.time() - start_time) / 60
    if duration > 10:
      raise data_quality.QualityCheckError(
          'Command should not take longer than 10 minutes.'
          ' Took {:.1f} minutes.'.format(duration))
