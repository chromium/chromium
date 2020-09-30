# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main Python API for analyzing binary size."""

import argparse
import bisect
import calendar
import collections
import datetime
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
import zipfile
import zlib

import apkanalyzer
import ar
import demangle
import describe
import file_format
import function_signature
import linker_map_parser
import models
import ninja_parser
import nm
import obj_analyzer
import parallel
import path_util
import string_extract
import zip_util


sys.path.insert(1, os.path.join(path_util.TOOLS_SRC_ROOT, 'tools', 'grit'))
from grit.format import data_pack

_METADATA_FILENAME = 'DIR_METADATA'
_METADATA_COMPONENT_REGEX = re.compile(r'^\s*component:\s*"(.*?)"',
                                       re.MULTILINE)
_OWNERS_FILENAME = 'OWNERS'
_OWNERS_COMPONENT_REGEX = re.compile(r'^\s*#\s*COMPONENT:\s*(\S+)',
                                     re.MULTILINE)
_OWNERS_FILE_PATH_REGEX = re.compile(r'^\s*file://(\S+)', re.MULTILINE)

_UNCOMPRESSED_COMPRESSION_RATIO_THRESHOLD = 0.9
_APKS_MAIN_APK = 'splits/base-master.apk'

# Holds computation state that is live only when an output directory exists.
_OutputDirectoryContext = collections.namedtuple('_OutputDirectoryContext', [
    'elf_object_paths',  # Only when elf_path is also provided.
    'known_inputs',  # Only when elf_path is also provided.
    'output_directory',
    'source_mapper',
    'thin_archives',
])

# When ensuring matching section sizes between .elf and .map files, these
# sections should be ignored. When lld creates a combined library with
# partitions, some sections (like .text) exist in each partition, but the ones
# below are common. At library splitting time, llvm-objcopy pulls what's needed
# from these sections into the new libraries. Hence, the ELF sections will end
# up smaller than the combined .map file sections.
_SECTION_SIZE_BLACKLIST = ['.symtab', '.shstrtab', '.strtab']


# Tunable constant "knobs" for CreateContainerAndSymbols().
class SectionSizeKnobs(object):
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
        '../../third_party/arcore-android-sdk',
        'lib/armeabi-v7a/libarcore_sdk_c.so':
        '../../third_party/arcore-android-sdk',
        'lib/armeabi-v7a/libcrashpad_handler_trampoline.so':
        '../../third_party/crashpad/libcrashpad_handler_trampoline.so',
        'lib/armeabi-v7a/libyoga.so':
        '../../chrome/android/feed',
        'lib/armeabi-v7a/libelements.so':
        '../../chrome/android/feed',
        'lib/arm64-v8a/libcrashpad_handler_trampoline.so':
        '../../third_party/crashpad/libcrashpad_handler_trampoline.so',
    }


# Parameters and states for archiving a container.
class ContainerArchiveOptions:
  def __init__(self, top_args, sub_args):
    # An estimate of pak translation compression ratio to make comparisons
    # between .size files reasonable. Otherwise this can differ every pak
    # change.
    self.pak_compression_ratio = 0.38 if sub_args.minimal_apks_file else 0.33

    # Whether to count number of relative relocations instead of binary size.
    self.relocations_mode = top_args.relocations

    self.analyze_java = not (sub_args.native_only or sub_args.no_java
                             or top_args.native_only or top_args.no_java
                             or self.relocations_mode)
    # This may be further disabled downstream, e.g., for the case where an APK
    # is specified, but it contains no .so files.
    self.analyze_native = not (sub_args.java_only or sub_args.no_native
                               or top_args.java_only or top_args.no_native)

    self.track_string_literals = True


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
    elif (full_name.startswith('*') or
        symbol.IsOverhead() or
        symbol.IsOther()):
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


def _ExtractSourcePathsAndNormalizeObjectPaths(raw_symbols, source_mapper):
  """Fills in the |source_path| attribute and normalizes |object_path|."""
  if source_mapper:
    logging.info('Looking up source paths from ninja files')
    for symbol in raw_symbols:
      object_path = symbol.object_path
      if symbol.IsDex() or symbol.IsOther():
        if symbol.source_path:
          symbol.generated_source, symbol.source_path = _NormalizeSourcePath(
              symbol.source_path)
      elif object_path:
        # We don't have source info for prebuilt .a files.
        if not os.path.isabs(object_path) and not object_path.startswith('..'):
          source_path = source_mapper.FindSourceForPath(object_path)
          if source_path:
            symbol.generated_source, symbol.source_path = (
                _NormalizeSourcePath(source_path))
        symbol.object_path = _NormalizeObjectPath(object_path)
    assert source_mapper.unmatched_paths_count == 0, (
        'One or more source file paths could not be found. Likely caused by '
        '.ninja files being generated at a different time than the .map file.')
  else:
    logging.info('Normalizing object paths')
    for symbol in raw_symbols:
      if symbol.object_path:
        symbol.object_path = _NormalizeObjectPath(symbol.object_path)


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
    # In order to achieve a total ordering in the presence of aliases, need to
    # include both |address| and |object_path|.
    # In order to achieve consistent deduping, need to include |size|.
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
    # Aliases come out in random order, so sort to be deterministic.
    ret[i].sort(key=lambda s: (s.address, s.object_path))

  logging.debug(
      'Removed %d overlapping string literals (%d bytes) & created %d aliases',
                num_removed, size_removed, num_aliases)
  return ret


def _ParseComponentFromMetadata(path):
  """Extracts Component from DIR_METADATA."""
  try:
    with open(path) as f:
      data = f.read()

    m = _METADATA_COMPONENT_REGEX.search(data)
    if m:
      return m.group(1)
  except IOError:
    # Need to catch both FileNotFoundError and NotADirectoryError since
    # source_paths for .aar files look like: /path/to/lib.aar/path/within/zip
    pass
  return ''


def _ParseComponentFromOwners(path):
  """Extracts COMPONENT and file:// from an OWNERS file.

  Args:
    path: Path to the file to parse.

  Returns:
    (component, None) if COMPONENT: line was found.
    ('', path) if a single file:// was found.
    ('', None) if neither was found.
  """
  try:
    with open(path) as f:
      data = f.read()

    m = _OWNERS_COMPONENT_REGEX.search(data)
    if m:
      return m.group(1), None
    aliases = _OWNERS_FILE_PATH_REGEX.findall(data)
    if len(aliases) == 1:
      return '', aliases[0]
  except IOError:
    # Need to catch both FileNotFoundError and NotADirectoryError since
    # source_paths for .aar files look like: /path/to/lib.aar/path/within/zip
    pass
  return '', None


def _FindComponentRoot(path, cache, source_directory):
  """Searches all parent directories for COMPONENT in OWNERS files.

  Args:
    path: Path of directory to start searching from. Must be relative to
      |source_directory|.
    cache: Dict of OWNERS paths. Used instead of filesystem if paths are present
      in the dict.
    source_directory: Directory to use as the root.

  Returns:
    COMPONENT belonging to |path|, or empty string if not found.
  """
  assert not os.path.isabs(path)
  component = cache.get(path)
  if component is not None:
    return component

  metadata_path = os.path.join(source_directory, path, _METADATA_FILENAME)
  component = _ParseComponentFromMetadata(metadata_path)
  if not component:
    owners_path = os.path.join(source_directory, path, _OWNERS_FILENAME)
    component, path_alias = _ParseComponentFromOwners(owners_path)

  if not component:
    # Store in cache before recursing to prevent cycles.
    cache[path] = ''
    if path_alias:
      alias_dir = os.path.dirname(path_alias)
      component = _FindComponentRoot(alias_dir, cache, source_directory)

  if not component:
    parent_path = os.path.dirname(path)
    if parent_path:
      component = _FindComponentRoot(parent_path, cache, source_directory)

  cache[path] = component
  return component


def _PopulateComponents(raw_symbols, source_directory):
  """Populates the |component| field based on |source_path|.

  Symbols without a |source_path| are skipped.

  Args:
    raw_symbols: list of Symbol objects.
    source_directory: Directory to use as the root.
  """
  seen_paths = {}
  for symbol in raw_symbols:
    if symbol.source_path:
      folder_path = os.path.dirname(symbol.source_path)
      symbol.component = _FindComponentRoot(folder_path, seen_paths,
                                            source_directory)


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
  _NormalizeNames(after_size_info.symbols)
  logging.info('Loaded %d + %d symbols', len(before_size_info.raw_symbols),
               len(after_size_info.raw_symbols))
  return before_size_info, after_size_info


def _CollectModuleSizes(minimal_apks_path):
  sizes_by_module = collections.defaultdict(int)
  with zipfile.ZipFile(minimal_apks_path) as z:
    for info in z.infolist():
      # E.g.:
      # splits/base-master.apk
      # splits/base-en.apk
      # splits/vr-master.apk
      # splits/vr-en.apk
      # TODO(agrieve): Might be worth measuring a non-en locale as well.
      m = re.match(r'splits/(.*)-master\.apk', info.filename)
      if m:
        sizes_by_module[m.group(1)] += info.file_size
  return sizes_by_module


def _ExtendSectionRange(section_range_by_name, section_name, delta_size):
  (prev_address, prev_size) = section_range_by_name.get(section_name, (0, 0))
  section_range_by_name[section_name] = (prev_address, prev_size + delta_size)


def CreateMetadata(args, linker_name, build_config):
  """Creates metadata dict while updating |build_config|.

  Args:
    args: Resolved command-line args.
    linker_name: A coded linker name (see linker_map_parser.py).
    build_config: Common build configurations to update or to undergo
        consistency checks.

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
  if args.output_directory:
    shorten_path = lambda path: os.path.relpath(path, args.output_directory)
    gn_args = _ParseGnArgs(os.path.join(args.output_directory, 'args.gn'))
    update_build_config(models.BUILD_CONFIG_GN_ARGS, gn_args)
  else:
    # If output directory is unavailable, just store basenames.
    shorten_path = os.path.basename

  if args.tool_prefix:
    relative_tool_prefix = path_util.ToToolsSrcRootRelative(args.tool_prefix)
    update_build_config(models.BUILD_CONFIG_TOOL_PREFIX, relative_tool_prefix)

  if linker_name:
    update_build_config(models.BUILD_CONFIG_LINKER_NAME, linker_name)

  # Deduce GIT revision.
  git_rev = _DetectGitRevision(args.source_directory)
  if git_rev:
    update_build_config(models.BUILD_CONFIG_GIT_REVISION, git_rev)

  if args.elf_file:
    metadata[models.METADATA_ELF_FILENAME] = shorten_path(args.elf_file)
    architecture = _ArchFromElf(args.elf_file, args.tool_prefix)
    metadata[models.METADATA_ELF_ARCHITECTURE] = architecture
    timestamp_obj = datetime.datetime.utcfromtimestamp(
        os.path.getmtime(args.elf_file))
    timestamp = calendar.timegm(timestamp_obj.timetuple())
    metadata[models.METADATA_ELF_MTIME] = timestamp
    build_id = BuildIdFromElf(args.elf_file, args.tool_prefix)
    metadata[models.METADATA_ELF_BUILD_ID] = build_id
    relocations_count = _CountRelocationsFromElf(args.elf_file,
                                                 args.tool_prefix)
    metadata[models.METADATA_ELF_RELOCATIONS_COUNT] = relocations_count

  if args.map_file:
    metadata[models.METADATA_MAP_FILENAME] = shorten_path(args.map_file)

  if args.minimal_apks_file:
    sizes_by_module = _CollectModuleSizes(args.minimal_apks_file)
    metadata[models.METADATA_APK_FILENAME] = shorten_path(
        args.minimal_apks_file)
    for name, size in sizes_by_module.items():
      key = models.METADATA_APK_SIZE
      if name != 'base':
        key += '-' + name
      metadata[key] = size
  elif args.apk_file:
    metadata[models.METADATA_APK_FILENAME] = shorten_path(args.apk_file)
    metadata[models.METADATA_APK_SIZE] = os.path.getsize(args.apk_file)

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


def _ParseElfInfo(map_path, elf_path, tool_prefix, track_string_literals,
                  outdir_context=None, linker_name=None):
  """Adds ELF section ranges and symbols."""
  if elf_path:
    # Run nm on the elf file to retrieve the list of symbol names per-address.
    # This list is required because the .map file contains only a single name
    # for each address, yet multiple symbols are often coalesced when they are
    # identical. This coalescing happens mainly for small symbols and for C++
    # templates. Such symbols make up ~500kb of libchrome.so on Android.
    elf_nm_result = nm.CollectAliasesByAddressAsync(elf_path, tool_prefix)

    # Run nm on all .o/.a files to retrieve the symbol names within them.
    # The list is used to detect when mutiple .o files contain the same symbol
    # (e.g. inline functions), and to update the object_path / source_path
    # fields accordingly.
    # Looking in object files is required because the .map file choses a
    # single path for these symbols.
    # Rather than record all paths for each symbol, set the paths to be the
    # common ancestor of all paths.
    if outdir_context:
      bulk_analyzer = obj_analyzer.BulkObjectFileAnalyzer(
          tool_prefix, outdir_context.output_directory,
          track_string_literals=track_string_literals)
      bulk_analyzer.AnalyzePaths(outdir_context.elf_object_paths)

  logging.info('Parsing Linker Map')
  with _OpenMaybeGzAsText(map_path) as map_file:
    map_section_ranges, raw_symbols, linker_map_extras = (
        linker_map_parser.MapFileParser().Parse(linker_name, map_file))

    if outdir_context and outdir_context.thin_archives:
      _ResolveThinArchivePaths(raw_symbols, outdir_context.thin_archives)

  if elf_path:
    logging.debug('Validating section sizes')
    elf_section_ranges = _SectionInfoFromElf(elf_path, tool_prefix)
    differing_elf_section_sizes = {}
    differing_map_section_sizes = {}
    for k, (_, elf_size) in elf_section_ranges.items():
      if k in _SECTION_SIZE_BLACKLIST:
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

  if elf_path and outdir_context:
    missed_object_paths = _DiscoverMissedObjectPaths(
        raw_symbols, outdir_context.known_inputs)
    missed_object_paths = ar.ExpandThinArchives(
        missed_object_paths, outdir_context.output_directory)[0]
    bulk_analyzer.AnalyzePaths(missed_object_paths)
    bulk_analyzer.SortPaths()
    if track_string_literals:
      merge_string_syms = [s for s in raw_symbols if
                           s.full_name == '** merge strings' or
                           s.full_name == '** lld merge strings']
      # More likely for there to be a bug in supersize than an ELF to not have a
      # single string literal.
      assert merge_string_syms
      string_ranges = [(s.address, s.size) for s in merge_string_syms]
      bulk_analyzer.AnalyzeStringLiterals(elf_path, string_ranges)

  # Map file for some reason doesn't demangle all names.
  # Demangle prints its own log statement.
  demangle.DemangleRemainingSymbols(raw_symbols, tool_prefix)

  object_paths_by_name = {}
  if elf_path:
    logging.info(
        'Adding symbols removed by identical code folding (as reported by nm)')
    # This normally does not block (it's finished by this time).
    names_by_address = elf_nm_result.get()
    _UpdateSymbolNamesFromNm(raw_symbols, names_by_address)

    raw_symbols = _AddNmAliases(raw_symbols, names_by_address)

    if outdir_context:
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

      if track_string_literals:
        logging.info('Waiting for string literal extraction to complete.')
        list_of_positions_by_object_path = bulk_analyzer.GetStringPositions()
      bulk_analyzer.Close()

      if track_string_literals:
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

  linker_map_parser.DeduceObjectPathsFromThinMap(raw_symbols, linker_map_extras)

  if elf_path:
    _NameStringLiterals(raw_symbols, elf_path, tool_prefix)

  # If we have an ELF file, use its ranges as the source of truth, since some
  # sections can differ from the .map.
  return (elf_section_ranges if elf_path else map_section_ranges, raw_symbols,
          object_paths_by_name)


def _ComputePakFileSymbols(
    file_name, contents, res_info, symbols_by_id, compression_ratio=1):
  id_map = {
      id(v): k
      for k, v in sorted(list(contents.resources.items()), reverse=True)
  }
  alias_map = {
      k: id_map[id(v)]
      for k, v in contents.resources.items() if id_map[id(v)] != k
  }
  # Longest locale pak is: es-419.pak.
  # Only non-translated .pak files are: resources.pak, chrome_100_percent.pak.
  if len(posixpath.basename(file_name)) <= 10:
    section_name = models.SECTION_PAK_TRANSLATIONS
  else:
    section_name = models.SECTION_PAK_NONTRANSLATED
  overhead = (12 + 6) * compression_ratio  # Header size plus extra offset
  # Key just needs to be unique from other IDs and pak overhead symbols.
  symbols_by_id[-len(symbols_by_id) - 1] = models.Symbol(
      section_name, overhead, full_name='Overhead: {}'.format(file_name))
  for resource_id in sorted(contents.resources):
    if resource_id in alias_map:
      # 4 extra bytes of metadata (2 16-bit ints)
      size = 4
      resource_id = alias_map[resource_id]
    else:
      resource_data = contents.resources[resource_id]
      # 6 extra bytes of metadata (1 32-bit int, 1 16-bit int)
      size = len(resource_data) + 6
      name, source_path = res_info[resource_id]
      if resource_id not in symbols_by_id:
        full_name = '{}: {}'.format(source_path, name)
        new_symbol = models.Symbol(
            section_name, 0, address=resource_id, full_name=full_name)
        if (section_name == models.SECTION_PAK_NONTRANSLATED and
            _IsPakContentUncompressed(resource_data)):
          new_symbol.flags |= models.FLAG_UNCOMPRESSED
        symbols_by_id[resource_id] = new_symbol

    size *= compression_ratio
    symbols_by_id[resource_id].size += size
  return section_name


def _IsPakContentUncompressed(content):
  raw_size = len(content)
  # Assume anything less than 100 bytes cannot be compressed.
  if raw_size < 100:
    return False

  compressed_size = len(zlib.compress(content, 1))
  compression_ratio = compressed_size / float(raw_size)
  return compression_ratio < _UNCOMPRESSED_COMPRESSION_RATIO_THRESHOLD


class _ResourceSourceMapper(object):
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


def _ParsePakInfoFile(pak_info_path):
  with open(pak_info_path, 'r') as info_file:
    res_info = {}
    for line in info_file.readlines():
      name, res_id, path = line.split(',')
      res_info[int(res_id)] = (name, path.strip())
  return res_info


def _ParsePakSymbols(symbols_by_id, object_paths_by_pak_id):
  raw_symbols = []
  for resource_id, symbol in symbols_by_id.items():
    raw_symbols.append(symbol)
    paths = object_paths_by_pak_id.get(resource_id)
    if not paths:
      continue
    symbol.object_path = paths.pop()
    if not paths:
      continue
    aliases = symbol.aliases or [symbol]
    symbol.aliases = aliases
    for path in paths:
      new_sym = models.Symbol(
          symbol.section_name, symbol.size, address=symbol.address,
          full_name=symbol.full_name, object_path=path, aliases=aliases)
      aliases.append(new_sym)
      raw_symbols.append(new_sym)
  # Sorting can ignore containers because symbols created here are all in the
  # same container.
  raw_symbols.sort(key=lambda s: (s.section_name, s.address, s.object_path))
  raw_total = 0.0
  int_total = 0
  for symbol in raw_symbols:
    raw_total += symbol.size
    # We truncate rather than round to ensure that we do not over attribute. It
    # is easier to add another symbol to make up the difference.
    symbol.size = int(symbol.size)
    int_total += symbol.size
  # Attribute excess to translations since only those are compressed.
  raw_symbols.append(models.Symbol(
      models.SECTION_PAK_TRANSLATIONS, int(round(raw_total - int_total)),
      full_name='Overhead: Pak compression artifacts'))
  return raw_symbols


def _ParseApkElfSectionRanges(section_ranges, metadata, apk_elf_result):
  if metadata:
    logging.debug('Extracting section sizes from .so within .apk')
    apk_build_id, apk_section_ranges, elf_overhead_size = apk_elf_result.get()
    assert apk_build_id == metadata[models.METADATA_ELF_BUILD_ID], (
        'BuildID from apk_elf_result did not match')

    packed_section_name = None
    architecture = metadata[models.METADATA_ELF_ARCHITECTURE]
    # Packing occurs enabled only arm32 & arm64.
    if architecture == 'arm':
      packed_section_name = '.rel.dyn'
    elif architecture == 'arm64':
      packed_section_name = '.rela.dyn'

    if packed_section_name:
      unpacked_range = section_ranges.get(packed_section_name)
      if unpacked_range is None:
        logging.warning('Packed section not present: %s', packed_section_name)
      elif unpacked_range != apk_section_ranges.get(packed_section_name):
        # These ranges are different only when using relocation_packer, which
        # hasn't been used since switching from gold -> lld.
        apk_section_ranges['%s (unpacked)' %
                           packed_section_name] = unpacked_range
  else:
    _, apk_section_ranges, elf_overhead_size = apk_elf_result.get()
  return apk_section_ranges, elf_overhead_size


class _ResourcePathDeobfuscator(object):

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
    # module.
    long_path = self._pathmap.get('base/{}'.format(path))
    if long_path:
      # The first 5 chars are 'base/', which we don't need because we are
      # looking directly inside the base module apk.
      return long_path[5:]
    return path


def _ParseApkOtherSymbols(section_ranges, apk_path, apk_so_path,
                          resources_pathmap_path, size_info_prefix, metadata,
                          knobs):
  res_source_mapper = _ResourceSourceMapper(size_info_prefix, knobs)
  resource_deobfuscator = _ResourcePathDeobfuscator(resources_pathmap_path)
  apk_symbols = []
  dex_size = 0
  zip_info_total = 0
  zipalign_total = 0
  with zipfile.ZipFile(apk_path) as z:
    signing_block_size = zip_util.MeasureApkSignatureBlock(z)
    for zip_info in z.infolist():
      zip_info_total += zip_info.compress_size
      # Account for zipalign overhead that exists in local file header.
      zipalign_total += zip_util.ReadZipInfoExtraFieldLength(z, zip_info)
      # Account for zipalign overhead that exists in central directory header.
      # Happens when python aligns entries in apkbuilder.py, but does not
      # exist when using Android's zipalign. E.g. for bundle .apks files.
      zipalign_total += len(zip_info.extra)
      # Skip main shared library, pak, and dex files as they are accounted for.
      if (zip_info.filename == apk_so_path
          or zip_info.filename.endswith('.pak')):
        continue
      if zip_info.filename.endswith('.dex'):
        dex_size += zip_info.file_size
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
  overhead_size = (os.path.getsize(apk_path) - zip_info_total - zipalign_total -
                   signing_block_size)
  assert overhead_size >= 0, 'Apk overhead must be non-negative'
  zip_overhead_symbol = models.Symbol(
      models.SECTION_OTHER, overhead_size, full_name='Overhead: APK file')
  apk_symbols.append(zip_overhead_symbol)
  _ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                      sum(s.size for s in apk_symbols))
  return dex_size, apk_symbols


def _CreatePakObjectMap(object_paths_by_name):
  # IDS_ macro usages result in templated function calls that contain the
  # resource ID in them. These names are collected along with all other symbols
  # by running "nm" on them. We just need to extract the values from them.
  object_paths_by_pak_id = {}
  PREFIX = 'void ui::AllowlistedResource<'
  id_start_idx = len(PREFIX)
  id_end_idx = -len('>()')
  for name in object_paths_by_name:
    if name.startswith(PREFIX):
      pak_id = int(name[id_start_idx:id_end_idx])
      object_paths_by_pak_id[pak_id] = object_paths_by_name[name]
  return object_paths_by_pak_id


def _FindPakSymbolsFromApk(opts, section_ranges, apk_path, size_info_prefix):
  with zipfile.ZipFile(apk_path) as z:
    pak_zip_infos = (f for f in z.infolist() if f.filename.endswith('.pak'))
    pak_info_path = size_info_prefix + '.pak.info'
    res_info = _ParsePakInfoFile(pak_info_path)
    symbols_by_id = {}
    total_compressed_size = 0
    total_uncompressed_size = 0
    for zip_info in pak_zip_infos:
      contents = data_pack.ReadDataPackFromString(z.read(zip_info))
      compression_ratio = 1.0
      if zip_info.compress_size < zip_info.file_size:
        total_compressed_size += zip_info.compress_size
        total_uncompressed_size += zip_info.file_size
        compression_ratio = opts.pak_compression_ratio
      section_name = _ComputePakFileSymbols(
          zip_info.filename, contents,
          res_info, symbols_by_id, compression_ratio=compression_ratio)
      _ExtendSectionRange(section_ranges, section_name, zip_info.compress_size)

    if total_uncompressed_size > 0:
      actual_ratio = (
          float(total_compressed_size) / total_uncompressed_size)
      logging.info(
          'Pak Compression Ratio: %f Actual: %f Diff: %.0f',
          opts.pak_compression_ratio, actual_ratio,
          (opts.pak_compression_ratio - actual_ratio) * total_uncompressed_size)
  return symbols_by_id


def _FindPakSymbolsFromFiles(section_ranges, pak_files, pak_info_path,
                             output_directory):
  """Uses files from args to find and add pak symbols."""
  res_info = _ParsePakInfoFile(pak_info_path)
  symbols_by_id = {}
  for pak_file_path in pak_files:
    with open(pak_file_path, 'rb') as f:
      contents = data_pack.ReadDataPackFromString(f.read())
    section_name = _ComputePakFileSymbols(
        os.path.relpath(pak_file_path, output_directory), contents, res_info,
        symbols_by_id)
    _ExtendSectionRange(section_ranges, section_name,
                        os.path.getsize(pak_file_path))
  return symbols_by_id


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


def _OverwriteSymbolSizesWithRelocationCount(raw_symbols, tool_prefix,
                                             elf_path):
  logging.info('Overwriting symbol sizes with relocation count')
  native_symbols = [sym for sym in raw_symbols if sym.IsNative()]
  symbol_addresses = [0] * (1 + len(native_symbols))

  for i, symbol in enumerate(native_symbols):
    symbol_addresses[i] = symbol.address

  # Last symbol address is the end of the last symbol, so we don't misattribute
  # all relros after the last symbol to that symbol.
  symbol_addresses[-1] = native_symbols[-1].address + native_symbols[-1].size

  for symbol in raw_symbols:
    symbol.address = 0
    symbol.size = 0
    symbol.padding = 0

  relocs_cmd = [path_util.GetReadElfPath(tool_prefix), '--relocs', elf_path]
  relro_addresses = subprocess.check_output(relocs_cmd).decode('ascii').split(
      '\n')
  # Grab first column from (sample output) '02de6d5c  00000017 R_ARM_RELATIVE'
  relro_addresses = [
      int(l.split()[0], 16) for l in relro_addresses if 'R_ARM_RELATIVE' in l
  ]
  # More likely for there to be a bug in supersize than an ELF to have any
  # relative relocations.
  assert relro_addresses

  logging.info('Adding %d relocations', len(relro_addresses))
  for addr in relro_addresses:
    # Attribute relros to largest symbol start address that precede them.
    idx = bisect.bisect_right(symbol_addresses, addr) - 1
    if 0 <= idx < len(native_symbols):
      symbol = native_symbols[idx]
      for alias in symbol.aliases or [symbol]:
        alias.size += 1

  logging.info('Removing non-native symbols...')
  raw_symbols[:] = [sym for sym in raw_symbols if sym.size or sym.IsNative()]


def _AddUnattributedSectionSymbols(raw_symbols, section_ranges):
  # Create symbols for ELF sections not covered by existing symbols.
  logging.info('Searching for symbol gaps...')
  last_symbol_ends = collections.defaultdict(int)
  for sym in raw_symbols:
    if sym.end_address > last_symbol_ends[sym.section_name]:
      last_symbol_ends[sym.section_name] = sym.end_address
  for section_name, last_symbol_end in last_symbol_ends.items():
    size_from_syms = last_symbol_end - section_ranges[section_name][0]
    overhead = section_ranges[section_name][1] - size_from_syms
    assert overhead >= 0, (
        ('End of last symbol (%x) in section %s is %d bytes after the end of '
         'section from readelf (%x).') % (last_symbol_end, section_name,
                                          -overhead,
                                          sum(section_ranges[section_name])))
    if overhead > 0 and section_name not in models.BSS_SECTIONS:
      raw_symbols.append(
          models.Symbol(
              section_name,
              overhead,
              address=last_symbol_end,
              full_name='** {} (unattributed)'.format(section_name)))
      logging.info('Last symbol in %s does not reach end of section, gap=%d',
                   section_name, overhead)

  # Sections that should not bundle into ".other".
  unsummed_sections, summed_sections = models.ClassifySections(
      section_ranges.keys())
  # Sort keys to ensure consistent order (> 1 sections may have address = 0).
  for section_name in sorted(section_ranges.keys()):
    # Handle sections that don't appear in |raw_symbols|.
    address, section_size = section_ranges[section_name]
    if (section_name not in unsummed_sections
        and section_name not in summed_sections):
      raw_symbols.append(
          models.Symbol(
              models.SECTION_OTHER,
              section_size,
              full_name='** ELF Section: {}'.format(section_name),
              address=address))
      _ExtendSectionRange(section_ranges, models.SECTION_OTHER, section_size)


def CreateContainerAndSymbols(knobs=None,
                              opts=None,
                              container_name=None,
                              metadata=None,
                              map_path=None,
                              tool_prefix=None,
                              output_directory=None,
                              source_directory=None,
                              elf_path=None,
                              apk_path=None,
                              mapping_path=None,
                              resources_pathmap_path=None,
                              apk_so_path=None,
                              pak_files=None,
                              pak_info_file=None,
                              linker_name=None,
                              size_info_prefix=None):
  """Creates a Container (with sections sizes) and symbols for a SizeInfo.

  Args:
    knobs: Instance of SectionSizeKnobs.
    opts: Instance of ContainerArchiveOptions.
    container_name: Name for the created Container. May be '' if only one
        Container exists.
    metadata: Metadata dict from CreateMetadata().
    map_path: Path to the linker .map(.gz) file to parse.
    tool_prefix: Prefix for c++filt & nm (required).
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    source_directory: Path to source root.
    elf_path: Path to the corresponding unstripped ELF file. Used to find symbol
        aliases and inlined functions. Can be None.
    apk_path: Path to the .apk file to measure.
    mapping_path: Path to the .mapping file for DEX symbol processing.
    resources_pathmap_path: Path to the pathmap file that maps original
        resource paths to shortened resource paths.
    apk_so_path: Path to an .so file within an APK file.
    pak_files: List of paths to .pak files.
    pak_info_file: Path to a .pak.info file.
    linker_name: A coded linker name (see linker_map_parser.py).
    size_info_prefix: Path to $out/size-info/$ApkName.

  Returns:
    A tuple of (container, raw_symbols).
    containers is a Container instance that stores metadata and section_sizes
    (section_sizes maps section names to respective sizes).
    raw_symbols is a list of Symbol objects.
  """
  knobs = knobs or SectionSizeKnobs()
  if apk_path and apk_so_path:
    # Extraction takes around 1 second, so do it in parallel.
    apk_elf_result = parallel.ForkAndCall(_ElfInfoFromApk,
                                          (apk_path, apk_so_path, tool_prefix))
  else:
    apk_elf_result = None

  outdir_context = None
  source_mapper = None
  if output_directory:
    # Start by finding the elf_object_paths, so that nm can run on them while
    # the linker .map is being parsed.
    logging.info('Parsing ninja files.')
    source_mapper, ninja_elf_object_paths = (
        ninja_parser.Parse(output_directory, elf_path))

    # If no symbols came from the library, it's because it's a partition
    # extracted from a combined library. Look there instead.
    if not ninja_elf_object_paths and elf_path:
      combined_elf_path = elf_path.replace('.so', '__combined.so')
      logging.info('Found no objects in %s, trying %s', elf_path,
                   combined_elf_path)
      source_mapper, ninja_elf_object_paths = (ninja_parser.Parse(
          output_directory, combined_elf_path))
      if ninja_elf_object_paths:
        assert map_path and '__combined.so.map' in map_path

    logging.debug('Parsed %d .ninja files.', source_mapper.parsed_file_count)
    assert not elf_path or ninja_elf_object_paths, (
        'Failed to find link command in ninja files for ' +
        os.path.relpath(elf_path, output_directory))

    if ninja_elf_object_paths:
      elf_object_paths, thin_archives = ar.ExpandThinArchives(
          ninja_elf_object_paths, output_directory)
      known_inputs = set(elf_object_paths)
      known_inputs.update(ninja_elf_object_paths)
    else:
      elf_object_paths = None
      known_inputs = None
      # When we don't know which elf file is used, just search all paths.
      if opts.analyze_native:
        thin_archives = set(
            p for p in source_mapper.IterAllPaths() if p.endswith('.a')
            and ar.IsThinArchive(os.path.join(output_directory, p)))
      else:
        thin_archives = None

    outdir_context = _OutputDirectoryContext(
        elf_object_paths=elf_object_paths,
        known_inputs=known_inputs,
        output_directory=output_directory,
        source_mapper=source_mapper,
        thin_archives=thin_archives)

  if opts.analyze_native:
    section_ranges, raw_symbols, object_paths_by_name = _ParseElfInfo(
        map_path,
        elf_path,
        tool_prefix,
        opts.track_string_literals,
        outdir_context=outdir_context,
        linker_name=linker_name)
  else:
    section_ranges, raw_symbols, object_paths_by_name = {}, [], None

  if apk_elf_result:
    section_ranges, elf_overhead_size = _ParseApkElfSectionRanges(
        section_ranges, metadata, apk_elf_result)
  elif elf_path:
    # Strip ELF before capturing section information to avoid recording
    # debug sections.
    with tempfile.NamedTemporaryFile(suffix=os.path.basename(elf_path)) as f:
      strip_path = path_util.GetStripPath(tool_prefix)
      subprocess.run([strip_path, '-o', f.name, elf_path], check=True)
      section_ranges = _SectionInfoFromElf(f.name, tool_prefix)
      elf_overhead_size = _CalculateElfOverhead(section_ranges, f.name)

  if elf_path:
    _AddUnattributedSectionSymbols(raw_symbols, section_ranges)

  pak_symbols_by_id = None
  if apk_path and size_info_prefix:
    # Can modify |section_ranges|.
    pak_symbols_by_id = _FindPakSymbolsFromApk(opts, section_ranges, apk_path,
                                               size_info_prefix)

    # Can modify |section_ranges|.
    dex_size, other_symbols = _ParseApkOtherSymbols(section_ranges, apk_path,
                                                    apk_so_path,
                                                    resources_pathmap_path,
                                                    size_info_prefix, metadata,
                                                    knobs)

    if opts.analyze_java:
      dex_symbols = apkanalyzer.CreateDexSymbols(apk_path, mapping_path,
                                                 size_info_prefix)
      raw_symbols.extend(dex_symbols)

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
        other_symbols.append(
            models.Symbol(
                models.SECTION_DEX,
                unattributed_dex,
                full_name='** .dex (unattributed - includes string literals)'))

    raw_symbols.extend(other_symbols)

  elif pak_files and pak_info_file:
    # Can modify |section_ranges|.
    pak_symbols_by_id = _FindPakSymbolsFromFiles(
        section_ranges, pak_files, pak_info_file, output_directory)

  if elf_path:
    elf_overhead_symbol = models.Symbol(
        models.SECTION_OTHER, elf_overhead_size, full_name='Overhead: ELF file')
    _ExtendSectionRange(section_ranges, models.SECTION_OTHER, elf_overhead_size)
    raw_symbols.append(elf_overhead_symbol)

  if pak_symbols_by_id:
    logging.debug('Extracting pak IDs from symbol names, and creating symbols')
    object_paths_by_pak_id = {}
    if opts.analyze_native:
      object_paths_by_pak_id = _CreatePakObjectMap(object_paths_by_name)
    pak_raw_symbols = _ParsePakSymbols(
        pak_symbols_by_id, object_paths_by_pak_id)
    raw_symbols.extend(pak_raw_symbols)

  _ExtractSourcePathsAndNormalizeObjectPaths(raw_symbols, source_mapper)
  _PopulateComponents(raw_symbols, source_directory)
  logging.info('Converting excessive aliases into shared-path symbols')
  _CompactLargeAliasesIntoSharedSymbols(raw_symbols, knobs)
  logging.debug('Connecting nm aliases')
  _ConnectNmAliases(raw_symbols)

  if elf_path and opts.relocations_mode:
    _OverwriteSymbolSizesWithRelocationCount(raw_symbols, tool_prefix, elf_path)

  section_sizes = {k: size for k, (address, size) in section_ranges.items()}
  container = models.Container(name=container_name,
                               metadata=metadata,
                               section_sizes=section_sizes)
  for symbol in raw_symbols:
    symbol.container = container
  return container, raw_symbols


def CreateSizeInfo(build_config,
                   container_list,
                   raw_symbols_list,
                   normalize_names=True):
  """Performs operations on all symbols and creates a SizeInfo object."""
  assert len(container_list) == len(raw_symbols_list)

  all_raw_symbols = []
  for raw_symbols in raw_symbols_list:
    file_format.SortSymbols(raw_symbols)
    file_format.CalculatePadding(raw_symbols)

    # Do not call _NormalizeNames() during archive since that method tends to
    # need tweaks over time. Calling it only when loading .size files allows for
    # more flexibility.
    if normalize_names:
      _NormalizeNames(raw_symbols)

    all_raw_symbols += raw_symbols

  return models.SizeInfo(build_config, container_list, all_raw_symbols)


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


def BuildIdFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-n', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  match = re.search(r'Build ID: (\w+)', stdout)
  assert match, 'Build ID not found from running: ' + ' '.join(args)
  return match.group(1)


def _SectionInfoFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-S', '--wide', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  section_ranges = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  for match in re.finditer(r'\[[\s\d]+\] (\..*)$', stdout, re.MULTILINE):
    items = match.group(1).split()
    section_ranges[items[0]] = (int(items[2], 16), int(items[4], 16))
  return section_ranges


def _ElfIsMainPartition(elf_path, tool_prefix):
  section_ranges = _SectionInfoFromElf(elf_path, tool_prefix)
  return models.SECTION_PART_END in section_ranges.keys()


def _ArchFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-h', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  machine = re.search('Machine:\s*(.+)', stdout).group(1)
  if machine == 'Intel 80386':
    return 'x86'
  if machine == 'Advanced Micro Devices X86-64':
    return 'x64'
  elif machine == 'ARM':
    return 'arm'
  elif machine == 'AArch64':
    return 'arm64'
  return machine


def _CountRelocationsFromElf(elf_path, tool_prefix):
  args = [path_util.GetObjDumpPath(tool_prefix), '--private-headers', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  relocations = re.search('REL[AR]?COUNT\s*(.+)', stdout).group(1)
  return int(relocations, 16)


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
  with _OpenMaybeGzAsText(map_path) as map_file:
    return linker_map_parser.DetectLinkerNameFromMapFile(map_file)


def _ElfInfoFromApk(apk_path, apk_so_path, tool_prefix):
  """Returns a tuple of (build_id, section_ranges, elf_overhead_size)."""
  with zip_util.UnzipToTemp(apk_path, apk_so_path) as temp:
    build_id = BuildIdFromElf(temp, tool_prefix)
    section_ranges = _SectionInfoFromElf(temp, tool_prefix)
    elf_overhead_size = _CalculateElfOverhead(section_ranges, temp)
    return build_id, section_ranges, elf_overhead_size


def _AddContainerArguments(parser):
  """Add arguments applicable to a single container."""

  # Special: Use _IdentifyInputFile() to detect main file argument.
  parser.add_argument('-f', metavar='FILE',
                      help='Auto-identify input file type.')

  # Main file argument: Exactly one should be specified (perhaps via -f), with
  # the exception that --map-file can be specified in addition.
  # _IdentifyInputFile() should be kept updated.
  parser.add_argument('--apk-file',
                      help='.apk file to measure. Other flags can generally be '
                      'derived when this is used.')
  parser.add_argument('--minimal-apks-file',
                      help='.minimal.apks file to measure. Other flags can '
                      'generally be derived when this is used.')
  parser.add_argument('--elf-file', help='Path to input ELF file.')
  parser.add_argument('--map-file',
                      help='Path to input .map(.gz) file. Defaults to '
                           '{{elf_file}}.map(.gz)?. If given without '
                           '--elf-file, no size metadata will be recorded.')

  # Auxiliary file arguments.
  parser.add_argument('--mapping-file',
                      help='Proguard .mapping file for deobfuscation.')
  parser.add_argument('--resources-pathmap-file',
                      help='.pathmap.txt file that contains a maping from '
                      'original resource paths to shortened resource paths.')
  parser.add_argument('--pak-file', action='append',
                      help='Paths to pak files.')
  parser.add_argument('--pak-info-file',
                      help='This file should contain all ids found in the pak '
                           'files that have been passed in.')
  parser.add_argument('--aux-elf-file',
                      help='Path to auxiliary ELF if the main file is APK, '
                      'useful for capturing metadata.')

  # Non-file argument.
  parser.add_argument('--no-string-literals', dest='track_string_literals',
                      default=True, action='store_false',
                      help='Disable breaking down "** merge strings" into more '
                           'granular symbols.')
  parser.add_argument(
      '--relocations',
      action='store_true',
      help='Instead of counting binary size, count number of relative'
      'relocation instructions in ELF code.')
  parser.add_argument(
      '--java-only', action='store_true', help='Run on only Java symbols')
  parser.add_argument(
      '--native-only', action='store_true', help='Run on only native symbols')
  parser.add_argument(
      '--no-java', action='store_true', help='Do not run on Java symbols')
  parser.add_argument(
      '--no-native', action='store_true', help='Do not run on native symbols')
  parser.add_argument(
      '--include-padding',
      action='store_true',
      help='Include a padding field for each symbol, instead of rederiving '
      'from consecutive symbols on file load.')


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  parser.add_argument('--source-directory',
                      help='Custom path to the root source directory.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('--tool-prefix',
                      help='Path prefix for c++filt, nm, readelf.')
  parser.add_argument(
      '--no-output-directory',
      action='store_true',
      help='Skips all data collection that requires build intermediates.')
  parser.add_argument('--ssargs-file',
                      help='Path to SuperSize multi-container arguments file.')
  _AddContainerArguments(parser)


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
    elif args.f.endswith('.ssargs'):
      # Fails if trying to nest them, which should never happen.
      args.ssargs_file = args.f
    else:
      on_config_error('Cannot identify file ' + args.f)
    args.f = None

  ret = [
      args.apk_file, args.elf_file, args.minimal_apks_file,
      args.__dict__.get('ssargs_file')
  ]
  ret = [v for v in ret if v]
  # --map-file can be a main file, or used with another main file.
  if not ret and args.map_file:
    ret.append(args.map_file)
  elif not ret:
    on_config_error(
        'Must pass at least one of --apk-file, --minimal-apks-file, '
        '--elf-file, --map-file, --ssargs-file')
  elif len(ret) > 1:
    on_config_error(
        'Found colliding --apk-file, --minimal-apk-file, --elf-file, '
        '--ssargs-file')
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


def _DeduceNativeInfo(tentative_output_dir, apk_path, elf_path, map_path,
                      on_config_error):
  apk_so_path = None
  if apk_path:
    with zipfile.ZipFile(apk_path) as z:
      lib_infos = [
          f for f in z.infolist()
          if f.filename.endswith('.so') and f.file_size > 0
      ]
    if not lib_infos:
      return None, map_path, None

    # TODO(agrieve): Add support for multiple .so files, and take into account
    #     secondary architectures.
    apk_so_path = max(lib_infos, key=lambda x: x.file_size).filename
    logging.debug('Sub-apk path=%s', apk_so_path)
    if not elf_path and tentative_output_dir:
      elf_path = os.path.join(
          tentative_output_dir, 'lib.unstripped',
          os.path.basename(apk_so_path.replace('crazy.', '')))
      logging.debug('Detected --elf-file=%s', elf_path)

  if map_path:
    if not map_path.endswith('.map') and not map_path.endswith('.map.gz'):
      on_config_error('Expected --map-file to end with .map or .map.gz')
  elif elf_path:
    # Look for a .map file named for either the ELF file, or in the
    # partitioned native library case, the combined ELF file from which the
    # main library was extracted. Note that we don't yet have |tool_prefix| to
    # use here, but that's not a problem for this use case.
    if _ElfIsMainPartition(elf_path, ''):
      map_path = elf_path.replace('.so', '__combined.so') + '.map'
    else:
      map_path = elf_path + '.map'
    if not os.path.exists(map_path):
      map_path += '.gz'

  if not os.path.exists(map_path):
    on_config_error(
        'Could not find .map(.gz)? file. Ensure you have built with '
        'is_official_build=true and generate_linker_map=true, or use '
        '--map-file to point me a linker map file.')

  return elf_path, map_path, apk_so_path


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
      if (k.endswith('_file') or k == 'f') and v is not None:
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


def _ProcessContainerArgs(top_args, sub_args, main_file, on_config_error):
  if hasattr(sub_args, 'name'):
    container_name = sub_args.name
  else:
    container_name = os.path.basename(main_file)
  if set(container_name) & set('<>'):
    parser.error('Container name cannot have characters in "<>"')

  # Copy output_directory, tool_prefix, etc. into sub_args.
  for k, v in top_args.__dict__.items():
    sub_args.__dict__.setdefault(k, v)

  opts = ContainerArchiveOptions(top_args, sub_args)
  apk_prefix = sub_args.minimal_apks_file or sub_args.apk_file
  if apk_prefix:
    # Allow either .minimal.apks or just .apks.
    apk_prefix = apk_prefix.replace('.minimal.apks', '.aab')
    apk_prefix = apk_prefix.replace('.apks', '.aab')

  sub_args.mapping_path, resources_pathmap_path = _DeduceAuxPaths(
      sub_args, apk_prefix)
  linker_name = None
  if opts.analyze_native:
    sub_args.elf_file, sub_args.map_file, apk_so_path = _DeduceNativeInfo(
        top_args.output_directory, sub_args.apk_file, sub_args.elf_file
        or sub_args.aux_elf_file, sub_args.map_file, on_config_error)
    if not (sub_args.elf_file or sub_args.map_file or apk_so_path):
      opts.analyze_native = False
  if opts.analyze_native:
    if sub_args.map_file:
      linker_name = _DetectLinkerName(sub_args.map_file)
      logging.info('Linker name: %s', linker_name)

      tool_prefix_finder = path_util.ToolPrefixFinder(
          value=sub_args.tool_prefix,
          output_directory=top_args.output_directory,
          linker_name=linker_name)
      sub_args.tool_prefix = tool_prefix_finder.Finalized()
  else:
    # Trust that these values will not be used, and set to None.
    sub_args.elf_file = None
    sub_args.map_file = None
    apk_so_path = None

  size_info_prefix = None
  if top_args.output_directory and apk_prefix:
    size_info_prefix = os.path.join(top_args.output_directory, 'size-info',
                                    os.path.basename(apk_prefix))

  container_args = {k: v for k, v in sub_args.__dict__.items()}
  container_args.update(opts.__dict__)
  logging.info('Container Params: %r', container_args)
  return (sub_args, opts, container_name, apk_so_path, resources_pathmap_path,
          linker_name, size_info_prefix)


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

    # If needed, extract .apk file to a temp file and process that instead.
    if sub_args.minimal_apks_file:
      with zip_util.UnzipToTemp(sub_args.minimal_apks_file,
                                _APKS_MAIN_APK) as temp:
        sub_args.apk_file = temp
        yield _ProcessContainerArgs(top_args, sub_args, main_file,
                                    on_config_error)
    else:
      yield _ProcessContainerArgs(top_args, sub_args, main_file,
                                  on_config_error)


def Run(top_args, on_config_error):
  if not top_args.size_file.endswith('.size'):
    on_config_error('size_file must end with .size')

  knobs = SectionSizeKnobs()
  build_config = {}
  seen_container_names = set()
  container_list = []
  raw_symbols_list = []
  # Iterate over each container.
  for (sub_args, opts, container_name, apk_so_path, resources_pathmap_path,
       linker_name, size_info_prefix) in _IterSubArgs(top_args,
                                                      on_config_error):

    if container_name in seen_container_names:
      raise ValueError('Duplicate container name: {}'.format(container_name))
    seen_container_names.add(container_name)

    metadata = CreateMetadata(sub_args, linker_name, build_config)
    container, raw_symbols = CreateContainerAndSymbols(
        knobs=knobs,
        opts=opts,
        container_name=container_name,
        metadata=metadata,
        map_path=sub_args.map_file,
        tool_prefix=sub_args.tool_prefix,
        elf_path=sub_args.elf_file,
        apk_path=sub_args.apk_file,
        mapping_path=sub_args.mapping_path,
        output_directory=sub_args.output_directory,
        source_directory=sub_args.source_directory,
        resources_pathmap_path=resources_pathmap_path,
        apk_so_path=apk_so_path,
        pak_files=sub_args.pak_file,
        pak_info_file=sub_args.pak_info_file,
        linker_name=linker_name,
        size_info_prefix=size_info_prefix)

    container_list.append(container)
    raw_symbols_list.append(raw_symbols)

  size_info = CreateSizeInfo(build_config,
                             container_list,
                             raw_symbols_list,
                             normalize_names=False)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in describe.DescribeSizeInfoCoverage(size_info):
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
