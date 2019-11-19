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
import string
import subprocess
import sys
import tempfile
import zipfile
import zlib

import apkanalyzer
import ar
import concurrent
import demangle
import describe
import file_format
import function_signature
import linker_map_parser
import models
import ninja_parser
import nm
import obj_analyzer
import path_util
import string_extract

sys.path.insert(1, os.path.join(path_util.SRC_ROOT, 'tools', 'grit'))
from grit.format import data_pack

_OWNERS_FILENAME = 'OWNERS'
_COMPONENT_REGEX = re.compile(r'\s*#\s*COMPONENT\s*:\s*(\S+)')
_FILE_PATH_REGEX = re.compile(r'\s*file://(\S+)')
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

# Tunable "knobs" for CreateSectionSizesAndSymbols().
class SectionSizeKnobs(object):
  def __init__(self, is_bundle=False):
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

    # An estimate of pak translation compression ratio to make comparisons
    # between .size files reasonable. Otherwise this can differ every pak
    # change.
    self.pak_compression_ratio = 0.38 if is_bundle else 0.33

    # File name: Source file.
    self.apk_other_files = {
      'assets/icudtl.dat': '../../third_party/icu/android/icudtl.dat',
      'assets/snapshot_blob_32.bin': '../../v8/snapshot_blob_32.bin',
      'assets/snapshot_blob_64.bin': '../../v8/snapshot_blob_64.bin',
      'assets/unwind_cfi_32': '../../base/trace_event/cfi_backtrace_android.cc',
      'assets/webapk_dex_version.txt': (
          '../../chrome/android/webapk/libs/runtime_library_version.gni'),
      'lib/armeabi-v7a/libarcore_sdk_c_minimal.so': (
          '../../third_party/arcore-android-sdk'),
      'lib/armeabi-v7a/libarcore_sdk_c.so': (
          '../../third_party/arcore-android-sdk'),
      'lib/armeabi-v7a/libcrashpad_handler_trampoline.so': (
          '../../third_party/crashpad/libcrashpad_handler_trampoline.so'),
      'lib/arm64-v8a/libcrashpad_handler_trampoline.so': (
          '../../third_party/crashpad/libcrashpad_handler_trampoline.so'),
    }

    self.apk_expected_other_files = {
      # From Monochrome.apk
      'AndroidManifest.xml',
      'resources.arsc',
      'assets/AndroidManifest.xml',
      'assets/metaresources.arsc',
      'META-INF/CERT.SF',
      'META-INF/CERT.RSA',
      'META-INF/CHROMIUM.SF',
      'META-INF/CHROMIUM.RSA',
      'META-INF/MANIFEST.MF',
    }

    self.analyze_java = True
    self.analyze_native = True

    self.src_root = path_util.SRC_ROOT

    # Whether to count number of relative relocations instead of binary size
    self.relocations_mode = False


def _OpenMaybeGz(path):
  """Calls `gzip.open()` if |path| ends in ".gz", otherwise calls `open()`."""
  if path.endswith('.gz'):
    return gzip.open(path, 'rb')
  return open(path, 'rb')


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
      if num_unknown_names < 10:
        logging.warning('Symbol not found in any .o files: %r', symbol)
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
  tups = itertools.izip(merge_string_syms, list_of_positions_by_object_path)
  for merge_sym, positions_by_object_path in tups:
    merge_sym_address = merge_sym.address
    new_symbols = []
    ret.append(new_symbols)
    for object_path, positions in positions_by_object_path.iteritems():
      for offset, size in positions:
        address = merge_sym_address + offset
        symbol = models.Symbol(
            models.SECTION_RODATA, size, address, STRING_LITERAL_NAME,
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


def _CalculatePadding(raw_symbols):
  """Populates the |padding| field based on symbol addresses.

  Symbols must already be sorted by |address|.
  """
  seen_sections = set()
  for i, symbol in enumerate(raw_symbols[1:]):
    prev_symbol = raw_symbols[i]
    if symbol.IsOverhead():
      # Overhead symbols are not actionable so should be padding-only.
      symbol.padding = symbol.size
    if prev_symbol.section_name != symbol.section_name:
      assert symbol.section_name not in seen_sections, (
          'Input symbols must be sorted by section, then address.')
      seen_sections.add(symbol.section_name)
      continue
    if (symbol.address <= 0 or prev_symbol.address <= 0 or
        not symbol.IsNative() or not prev_symbol.IsNative()):
      continue

    if symbol.address == prev_symbol.address:
      if symbol.aliases and symbol.aliases is prev_symbol.aliases:
        symbol.padding = prev_symbol.padding
        symbol.size = prev_symbol.size
        continue
      # Padding-only symbols happen for ** symbol gaps.
      assert prev_symbol.size_without_padding == 0, (
          'Found duplicate symbols:\n%r\n%r' % (prev_symbol, symbol))

    padding = symbol.address - prev_symbol.end_address
    symbol.padding = padding
    symbol.size += padding
    assert symbol.size >= 0, (
        'Symbol has negative size (likely not sorted propertly): '
        '%r\nprev symbol: %r' % (symbol, prev_symbol))


def _ParseComponentFromOwners(filename):
  """Searches an OWNERS file for lines that start with `# COMPONENT:`.

  If an OWNERS file has no COMPONENT but references another OWNERS file, follow
  the reference and check that file instead.

  Args:
    filename: Path to the file to parse.
  Returns:
    The text that follows the `# COMPONENT:` prefix, such as 'component>name'.
    Empty string if no component found or the file didn't exist.
  """
  reference_paths = []
  try:
    with open(filename) as f:
      for line in f:
        component_matches = _COMPONENT_REGEX.match(line)
        path_matches = _FILE_PATH_REGEX.match(line)
        if component_matches:
          return component_matches.group(1)
        elif path_matches:
          reference_paths.append(path_matches.group(1))
  except IOError:
    return ''

  if len(reference_paths) == 1:
    newpath = os.path.join(path_util.SRC_ROOT, reference_paths[0])
    return _ParseComponentFromOwners(newpath)
  else:
    return ''


def _FindComponentRoot(start_path, cache, knobs):
  """Searches all parent directories for COMPONENT in OWNERS files.

  Args:
    start_path: Path of directory to start searching from. Must be relative to
      SRC_ROOT.
    cache: Dict of OWNERS paths. Used instead of filesystem if paths are present
      in the dict.
    knobs: Instance of SectionSizeKnobs with tunable knobs and options.

  Returns:
    COMPONENT belonging to |start_path|, or empty string if not found.
  """
  prev_dir = None
  test_dir = start_path
  # This loop will traverse the directory structure upwards until reaching
  # SRC_ROOT, where test_dir and prev_dir will both equal an empty string.
  while test_dir != prev_dir:
    cached_component = cache.get(test_dir)
    if cached_component:
      return cached_component
    elif cached_component is None:
      owners_path = os.path.join(knobs.src_root, test_dir, _OWNERS_FILENAME)
      component = _ParseComponentFromOwners(owners_path)
      cache[test_dir] = component
      if component:
        return component
    prev_dir = test_dir
    test_dir = os.path.dirname(test_dir)
  return ''


def _PopulateComponents(raw_symbols, knobs):
  """Populates the |component| field based on |source_path|.

  Symbols without a |source_path| are skipped.

  Args:
    raw_symbols: list of Symbol objects.
    knobs: Instance of SectionSizeKnobs. Tunable knobs and options.
  """
  seen_paths = {}
  for symbol in raw_symbols:
    if symbol.source_path:
      folder_path = os.path.dirname(symbol.source_path)
      symbol.component = _FindComponentRoot(folder_path, seen_paths, knobs)


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
        missing_names[s.full_name].append(s.address)
        logging.warning('Name missing from aliases: %08x %s %s', s.address,
                        s.full_name, name_list)
        continue
      replacements.append((i, name_list))
      num_new_symbols += len(name_list) - 1

  if missing_names and logging.getLogger().isEnabledFor(logging.INFO):
    for address, names in names_by_address.iteritems():
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
  logging.info('Calculating padding')
  _CalculatePadding(size_info.raw_symbols)
  logging.info('Loaded %d symbols', len(size_info.raw_symbols))
  return size_info


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


def CreateMetadata(map_path, elf_path, apk_path, minimal_apks_path,
                   tool_prefix, output_directory, linker_name):
  """Creates metadata dict.

  Args:
    map_path: Path to the linker .map(.gz) file to parse.
    elf_path: Path to the corresponding unstripped ELF file. Used to find symbol
        aliases and inlined functions. Can be None.
    apk_path: Path to the .apk file to measure.
    minimal_apks_path: Path to the .minimal.apks file to measure.
    tool_prefix: Prefix for c++filt & nm.
    output_directory: Build output directory.
    linker_name: A coded linker name (see linker_map_parser.py).

  Returns:
    None if |elf_path| is not supplied. Otherwise returns dict mapping string
    constants to values.
    If |elf_path| is supplied, git revision and elf info are included.
    If |output_directory| is also supplied, then filenames will be included.
  """
  assert not (apk_path and minimal_apks_path)
  metadata = None
  if elf_path:
    logging.debug('Constructing metadata')
    git_rev = _DetectGitRevision(os.path.dirname(elf_path))
    architecture = _ArchFromElf(elf_path, tool_prefix)
    build_id = BuildIdFromElf(elf_path, tool_prefix)
    timestamp_obj = datetime.datetime.utcfromtimestamp(os.path.getmtime(
        elf_path))
    timestamp = calendar.timegm(timestamp_obj.timetuple())
    relative_tool_prefix = path_util.ToSrcRootRelative(tool_prefix)
    relocations_count = _CountRelocationsFromElf(elf_path, tool_prefix)

    metadata = {
        models.METADATA_GIT_REVISION: git_rev,
        models.METADATA_ELF_ARCHITECTURE: architecture,
        models.METADATA_ELF_MTIME: timestamp,
        models.METADATA_ELF_BUILD_ID: build_id,
        models.METADATA_LINKER_NAME: linker_name,
        models.METADATA_TOOL_PREFIX: relative_tool_prefix,
        models.METADATA_ELF_RELOCATIONS_COUNT: relocations_count
    }

    if output_directory:
      relative_to_out = lambda path: os.path.relpath(path, output_directory)
      gn_args = _ParseGnArgs(os.path.join(output_directory, 'args.gn'))
      metadata[models.METADATA_MAP_FILENAME] = relative_to_out(map_path)
      metadata[models.METADATA_ELF_FILENAME] = relative_to_out(elf_path)
      metadata[models.METADATA_GN_ARGS] = gn_args

      if apk_path:
        metadata[models.METADATA_APK_FILENAME] = relative_to_out(apk_path)
        metadata[models.METADATA_APK_SIZE] = os.path.getsize(apk_path)
      elif minimal_apks_path:
        sizes_by_module = _CollectModuleSizes(minimal_apks_path)
        metadata[models.METADATA_APK_FILENAME] = relative_to_out(
            minimal_apks_path)
        for name, size in sizes_by_module.iteritems():
          key = models.METADATA_APK_SIZE
          if name != 'base':
            key += '-' + name
          metadata[key] = size
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

  for sym, name in string_extract.ReadStringLiterals(raw_symbols, elf_path,
                                                     tool_prefix):
    # Newlines and tabs are used as delimiters in file_format.py
    # At this point, names still have a terminating null byte.
    name = name.translate(None, '\t\n').strip('\00')
    is_printable = all(c in string.printable for c in name)
    if not is_printable:
      sym.full_name = models.STRING_LITERAL_NAME
    elif len(name) > STRING_LENGTH_CUTOFF:
      sym.full_name = '"{}[...]"'.format(name[:STRING_LENGTH_CUTOFF])
    else:
      sym.full_name = '"{}"'.format(name)


def _ParseElfInfo(map_path, elf_path, tool_prefix, track_string_literals,
                  outdir_context=None, linker_name=None):
  """Adds ELF section sizes and symbols."""
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
  with _OpenMaybeGz(map_path) as map_file:
    map_section_sizes, raw_symbols, linker_map_extras = (
        linker_map_parser.MapFileParser().Parse(linker_name, map_file))

    if outdir_context and outdir_context.thin_archives:
      _ResolveThinArchivePaths(raw_symbols, outdir_context.thin_archives)

  if elf_path:
    logging.debug('Validating section sizes')
    _, elf_section_sizes = _SectionInfoFromElf(elf_path, tool_prefix)
    differing_elf_section_sizes = {}
    differing_map_section_sizes = {}
    for k, v in elf_section_sizes.iteritems():
      if k not in _SECTION_SIZE_BLACKLIST and v != map_section_sizes.get(k):
        differing_map_section_sizes[k] = map_section_sizes.get(k)
        differing_elf_section_sizes[k] = v
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
        for merge_sym, literal_syms in itertools.izip(
            merge_string_syms, replacements):
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

  # If we have an ELF file, use its sizes as the source of truth, since some
  # sections can differ from the .map.
  return (elf_section_sizes if elf_path else map_section_sizes, raw_symbols,
          object_paths_by_name)


def _ComputePakFileSymbols(
    file_name, contents, res_info, symbols_by_id, compression_ratio=1):
  id_map = {id(v): k
            for k, v in sorted(contents.resources.items(), reverse=True)}
  alias_map = {k: id_map[id(v)] for k, v in contents.resources.iteritems()
               if id_map[id(v)] != k}
  # Longest locale pak is: es-419.pak.
  # Only non-translated .pak files are: resources.pak, chrome_100_percent.pak.
  if len(posixpath.basename(file_name)) <= 10:
    section_name = models.SECTION_PAK_TRANSLATIONS
  else:
    section_name = models.SECTION_PAK_NONTRANSLATED
  overhead = (12 + 6) * compression_ratio  # Header size plus extra offset
  symbols_by_id[hash(file_name)] = models.Symbol(
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
      res_info = {}
      renames = {}
      for line in info_file.readlines():
        dest, source = line.strip().split(',')
        # Allow indirection due to renames.
        if dest.startswith('Rename:'):
          dest = dest.split(':', 1)[1]
          renames[dest] = source
        else:
          res_info[dest] = source
      for dest, renamed_dest in renames.iteritems():
        # Allow one more level of indirection due to renaming renamed files
        renamed_dest = renames.get(renamed_dest, renamed_dest)
        actual_source = res_info.get(renamed_dest)
        if actual_source:
          res_info[dest] = actual_source
    return res_info

  def _LoadResInfo(self, size_info_prefix):
    apk_res_info_path = size_info_prefix + '.res.info'
    res_info_without_root = self._ParseResInfoFile(apk_res_info_path)
    # We package resources in the res/ folder only in the apk.
    res_info = {
        os.path.join('res', dest): source
        for dest, source in res_info_without_root.iteritems()
    }
    res_info.update(self._knobs.apk_other_files)
    return res_info

  def FindSourceForPath(self, path):
    original_path = path
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
    if original_path not in self._knobs.apk_expected_other_files:
      logging.warning('Unexpected file in apk: %s', original_path)
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
  for resource_id, symbol in symbols_by_id.iteritems():
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


def _ParseApkElfSectionSize(section_sizes, metadata, apk_elf_result):
  if metadata:
    logging.debug('Extracting section sizes from .so within .apk')
    apk_build_id, _, apk_section_sizes, elf_overhead_size = apk_elf_result.get()
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
      unpacked_size = section_sizes.get(packed_section_name)
      if unpacked_size is None:
        logging.warning('Packed section not present: %s', packed_section_name)
      elif unpacked_size != apk_section_sizes.get(packed_section_name):
        # These sizes are different only when using relocation_packer, which
        # hasn't been used since switching from gold -> lld.
        apk_section_sizes['%s (unpacked)' % packed_section_name] = unpacked_size
    return apk_section_sizes, elf_overhead_size
  return section_sizes, 0


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
    # module.
    long_path = self._pathmap.get('base/{}'.format(path))
    if long_path:
      # The first 5 chars are 'base/', which we don't need because we are
      # looking directly inside the base module apk.
      return long_path[5:]
    return path


def _ParseApkOtherSymbols(section_sizes, apk_path, apk_so_path,
                          resources_pathmap_path, size_info_prefix, knobs):
  res_source_mapper = _ResourceSourceMapper(size_info_prefix, knobs)
  resource_deobfuscator = _ResourcePathDeobfuscator(resources_pathmap_path)
  apk_symbols = []
  dex_size = 0
  zip_info_total = 0
  with zipfile.ZipFile(apk_path) as z:
    for zip_info in z.infolist():
      zip_info_total += zip_info.compress_size
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
  overhead_size = os.path.getsize(apk_path) - zip_info_total
  assert overhead_size >= 0, 'Apk overhead must be non-negative'
  zip_overhead_symbol = models.Symbol(
      models.SECTION_OTHER, overhead_size, full_name='Overhead: APK file')
  apk_symbols.append(zip_overhead_symbol)
  prev = section_sizes.setdefault(models.SECTION_OTHER, 0)
  section_sizes[models.SECTION_OTHER] = prev + sum(s.size for s in apk_symbols)

  return dex_size, apk_symbols


def _CreatePakObjectMap(object_paths_by_name):
  # IDS_ macro usages result in templated function calls that contain the
  # resource ID in them. These names are collected along with all other symbols
  # by running "nm" on them. We just need to extract the values from them.
  object_paths_by_pak_id = {}
  PREFIX = 'void ui::WhitelistedResource<'
  id_start_idx = len(PREFIX)
  id_end_idx = -len('>()')
  for name in object_paths_by_name:
    if name.startswith(PREFIX):
      pak_id = int(name[id_start_idx:id_end_idx])
      object_paths_by_pak_id[pak_id] = object_paths_by_name[name]
  return object_paths_by_pak_id


def _FindPakSymbolsFromApk(section_sizes, apk_path, size_info_prefix, knobs):
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
        compression_ratio = knobs.pak_compression_ratio
      section_name = _ComputePakFileSymbols(
          zip_info.filename, contents,
          res_info, symbols_by_id, compression_ratio=compression_ratio)
      prev_size = section_sizes.get(section_name, 0)
      section_sizes[section_name] = prev_size + zip_info.compress_size

    if total_uncompressed_size > 0:
      actual_ratio = (
          float(total_compressed_size) / total_uncompressed_size)
      logging.info('Pak Compression Ratio: %f Actual: %f Diff: %.0f',
          knobs.pak_compression_ratio, actual_ratio,
          (knobs.pak_compression_ratio - actual_ratio) *
              total_uncompressed_size)
  return symbols_by_id


def _FindPakSymbolsFromFiles(
    section_sizes, pak_files, pak_info_path, output_directory):
  """Uses files from args to find and add pak symbols."""
  res_info = _ParsePakInfoFile(pak_info_path)
  symbols_by_id = {}
  for pak_file_path in pak_files:
    with open(pak_file_path, 'r') as f:
      contents = data_pack.ReadDataPackFromString(f.read())
    section_name = _ComputePakFileSymbols(
        os.path.relpath(pak_file_path, output_directory), contents, res_info,
        symbols_by_id)
    prev_size = section_sizes.get(section_name, 0)
    section_sizes[section_name] = prev_size + os.path.getsize(pak_file_path)
  return symbols_by_id


def _CalculateElfOverhead(section_sizes, elf_path):
  if elf_path:
    section_sizes_total_without_bss = sum(
        s for k, s in section_sizes.iteritems() if k not in models.BSS_SECTIONS)
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
  relro_addresses = subprocess.check_output(relocs_cmd).split('\n')
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


def _AddUnattributedSectionSymbols(raw_symbols, section_sizes, elf_result):
  # Create symbols for ELF sections not covered by existing symbols.
  logging.info('Searching for symbol gaps...')
  section_addresses = elf_result.get()[1]
  last_symbol_ends = collections.defaultdict(int)
  for sym in raw_symbols:
    if sym.end_address > last_symbol_ends[sym.section_name]:
      last_symbol_ends[sym.section_name] = sym.end_address
  for section_name, last_symbol_end in last_symbol_ends.iteritems():
    size_from_syms = last_symbol_end - section_addresses[section_name]
    overhead = section_sizes[section_name] - size_from_syms
    assert overhead >= 0, (
        ('End of last symbol (%x) in section %s is %d bytes after the end of '
         'section from readelf (%x).') %
        (last_symbol_end, section_name, -overhead,
         section_sizes[section_name] + section_addresses[section_name]))
    if overhead > 0 and section_name not in models.BSS_SECTIONS:
      raw_symbols.append(
          models.Symbol(
              section_name,
              overhead,
              address=last_symbol_end,
              full_name='** {} (unattributed)'.format(section_name)))
      logging.info('Last symbol in %s does not reach end of section, gap=%d',
                   section_name, overhead)

  for section_name in section_addresses:
    # Handle sections that don't appear in |raw_symbols|.
    if section_name not in last_symbol_ends:
      section_size = section_sizes[section_name]
      logging.info('All bytes in %s are unattributed, gap=%d', section_name,
                   overhead)
      raw_symbols.append(
          models.Symbol(
              models.SECTION_OTHER,
              section_size,
              full_name='** ELF Section: {}'.format(section_name)))
      prev = section_sizes.setdefault(models.SECTION_OTHER, 0)
      section_sizes[models.SECTION_OTHER] = prev + section_size


def CreateSectionSizesAndSymbols(map_path=None,
                                 tool_prefix=None,
                                 output_directory=None,
                                 elf_path=None,
                                 apk_path=None,
                                 mapping_path=None,
                                 resources_pathmap_path=None,
                                 track_string_literals=True,
                                 metadata=None,
                                 apk_so_path=None,
                                 pak_files=None,
                                 pak_info_file=None,
                                 linker_name=None,
                                 size_info_prefix=None,
                                 knobs=None):
  """Creates sections sizes and symbols for a SizeInfo.

  Args:
    map_path: Path to the linker .map(.gz) file to parse.
    tool_prefix: Prefix for c++filt & nm (required).
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    elf_path: Path to the corresponding unstripped ELF file. Used to find symbol
        aliases and inlined functions. Can be None.
    apk_path: Path to the .apk file to measure.
    resources_pathmap_path: Path to the pathmap file that maps original
        resource paths to shortened resource paths.
    track_string_literals: Whether to break down "** merge string" sections into
        smaller symbols (requires output_directory).
    metadata: Metadata dict from CreateMetadata().
    apk_so_path: Path to an .so file within an APK file.
    pak_files: List of paths to .pak files.
    pak_info_file: Path to a .pak.info file.
    linker_name: A coded linker name (see linker_map_parser.py).
    size_info_prefix: Path to $out/size-info/$ApkName.
    knobs: Instance of SectionSizeKnobs with tunable knobs and options.

  Returns:
    A tuple of (section_sizes, raw_symbols).
    section_sizes is a dict mapping section names to their size
    raw_symbols is a list of Symbol objects
  """
  knobs = knobs or SectionSizeKnobs()
  if apk_path and elf_path:
    # Extraction takes around 1 second, so do it in parallel.
    apk_elf_result = concurrent.ForkAndCall(
        _ElfInfoFromApk, (apk_path, apk_so_path, tool_prefix))

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
      if knobs.analyze_native:
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

  if knobs.analyze_native:
    section_sizes, raw_symbols, object_paths_by_name = _ParseElfInfo(
        map_path,
        elf_path,
        tool_prefix,
        track_string_literals,
        outdir_context=outdir_context,
        linker_name=linker_name)
  else:
    section_sizes, raw_symbols, object_paths_by_name = {}, [], None

  elf_overhead_size = _CalculateElfOverhead(section_sizes, elf_path)

  pak_symbols_by_id = None
  if apk_path and size_info_prefix:
    if elf_path:
      section_sizes, elf_overhead_size = _ParseApkElfSectionSize(
          section_sizes, metadata, apk_elf_result)
      _AddUnattributedSectionSymbols(raw_symbols, section_sizes, apk_elf_result)

    pak_symbols_by_id = _FindPakSymbolsFromApk(
        section_sizes, apk_path, size_info_prefix, knobs)

    dex_size, other_symbols = _ParseApkOtherSymbols(
        section_sizes, apk_path, apk_so_path, resources_pathmap_path,
        size_info_prefix, knobs)

    if knobs.analyze_java:
      dex_symbols = apkanalyzer.CreateDexSymbols(
          apk_path, mapping_path, size_info_prefix, output_directory)
      raw_symbols.extend(dex_symbols)

      # We can't meaningfully track section size of dex methods vs other, so
      # just fake the size of dex methods as the sum of symbols, and make
      # "dex other" responsible for any unattributed bytes.
      dex_method_size = int(
          round(
              sum(s.pss for s in dex_symbols
                  if s.section_name == models.SECTION_DEX_METHOD)))
      section_sizes[models.SECTION_DEX_METHOD] = dex_method_size
      section_sizes[models.SECTION_DEX] = dex_size - dex_method_size

      dex_other_size = int(
          round(
              sum(s.pss for s in dex_symbols
                  if s.section_name == models.SECTION_DEX)))
      unattributed_dex = section_sizes[models.SECTION_DEX] - dex_other_size
      # Compare against -5 instead of 0 to guard against round-off errors.
      assert unattributed_dex >= -5, ('Dex symbols take up more space than '
                                      'the dex sections have available')
      if unattributed_dex > 0:
        other_symbols.append(
            models.Symbol(
                models.SECTION_DEX,
                unattributed_dex,
                full_name='** .dex (unattributed)'))

    raw_symbols.extend(other_symbols)

  elif pak_files and pak_info_file:
    pak_symbols_by_id = _FindPakSymbolsFromFiles(
        section_sizes, pak_files, pak_info_file, output_directory)

  if elf_path:
    elf_overhead_symbol = models.Symbol(
        models.SECTION_OTHER, elf_overhead_size, full_name='Overhead: ELF file')
    prev = section_sizes.setdefault(models.SECTION_OTHER, 0)
    section_sizes[models.SECTION_OTHER] = prev + elf_overhead_size
    raw_symbols.append(elf_overhead_symbol)

  if pak_symbols_by_id:
    logging.debug('Extracting pak IDs from symbol names, and creating symbols')
    object_paths_by_pak_id = {}
    if knobs.analyze_native:
      object_paths_by_pak_id = _CreatePakObjectMap(object_paths_by_name)
    pak_raw_symbols = _ParsePakSymbols(
        pak_symbols_by_id, object_paths_by_pak_id)
    raw_symbols.extend(pak_raw_symbols)

  _ExtractSourcePathsAndNormalizeObjectPaths(raw_symbols, source_mapper)
  _PopulateComponents(raw_symbols, knobs)
  logging.info('Converting excessive aliases into shared-path symbols')
  _CompactLargeAliasesIntoSharedSymbols(raw_symbols, knobs)
  logging.debug('Connecting nm aliases')
  _ConnectNmAliases(raw_symbols)

  if elf_path and knobs.relocations_mode:
    _OverwriteSymbolSizesWithRelocationCount(raw_symbols, tool_prefix, elf_path)

  return section_sizes, raw_symbols


def CreateSizeInfo(
    section_sizes, raw_symbols, metadata=None, normalize_names=True):
  """Performs operations on all symbols and creates a SizeInfo object."""
  logging.debug('Sorting %d symbols', len(raw_symbols))
  # TODO(agrieve): Either change this sort so that it's only sorting by section
  #     (and not using .sort()), or have it specify a total ordering (which must
  #     also include putting padding-only symbols before others of the same
  #     address). Note: The sort as-is takes ~1.5 seconds.
  raw_symbols.sort(key=lambda s: (
      s.IsPak(), s.IsBss(), s.section_name, s.address))
  logging.info('Processed %d symbols', len(raw_symbols))

  # Padding not really required, but it is useful to check for large padding and
  # log a warning.
  logging.info('Calculating padding')
  _CalculatePadding(raw_symbols)

  # Do not call _NormalizeNames() during archive since that method tends to need
  # tweaks over time. Calling it only when loading .size files allows for more
  # flexibility.
  if normalize_names:
    _NormalizeNames(raw_symbols)

  return models.SizeInfo(section_sizes, raw_symbols, metadata=metadata)


def _DetectGitRevision(directory):
  """Runs git rev-parse to get the SHA1 hash of the current revision.

  Args:
    directory: Path to directory where rev-parse command will be run.

  Returns:
    A string with the SHA1 hash, or None if an error occured.
  """
  try:
    git_rev = subprocess.check_output(
        ['git', '-C', directory, 'rev-parse', 'HEAD'])
    return git_rev.rstrip()
  except Exception:
    logging.warning('Failed to detect git revision for file metadata.')
    return None


def BuildIdFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-n', elf_path]
  stdout = subprocess.check_output(args)
  match = re.search(r'Build ID: (\w+)', stdout)
  assert match, 'Build ID not found from running: ' + ' '.join(args)
  return match.group(1)


def _SectionInfoFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-S', '--wide', elf_path]
  stdout = subprocess.check_output(args)
  section_addresses = {}
  section_sizes = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  for match in re.finditer(r'\[[\s\d]+\] (\..*)$', stdout, re.MULTILINE):
    items = match.group(1).split()
    section_addresses[items[0]] = int(items[2], 16)
    section_sizes[items[0]] = int(items[4], 16)
  return section_addresses, section_sizes


def _ElfIsMainPartition(elf_path, tool_prefix):
  _, section_sizes = _SectionInfoFromElf(elf_path, tool_prefix)
  return models.SECTION_PART_END in section_sizes.keys()


def _ArchFromElf(elf_path, tool_prefix):
  args = [path_util.GetReadElfPath(tool_prefix), '-h', elf_path]
  stdout = subprocess.check_output(args)
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
  stdout = subprocess.check_output(args)
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
  return ["%s=%s" % x for x in sorted(args.iteritems())]


def _DetectLinkerName(map_path):
  with _OpenMaybeGz(map_path) as map_file:
    return linker_map_parser.DetectLinkerNameFromMapFile(map_file)


def _ElfInfoFromApk(apk_path, apk_so_path, tool_prefix):
  """Returns a tuple of (build_id, section_sizes, elf_overhead_size)."""
  with zipfile.ZipFile(apk_path) as apk, \
       tempfile.NamedTemporaryFile() as f:
    f.write(apk.read(apk_so_path))
    f.flush()
    build_id = BuildIdFromElf(f.name, tool_prefix)
    section_addresses, section_sizes = _SectionInfoFromElf(f.name, tool_prefix)
    elf_overhead_size = _CalculateElfOverhead(section_sizes, f.name)
    return build_id, section_addresses, section_sizes, elf_overhead_size


def _AutoIdentifyInputFile(args):
  if args.f.endswith('.minimal.apks'):
    args.minimal_apks_file = args.f
    logging.info('Auto-identified --minimal-apks-file.')
  elif args.f.endswith('.apk'):
    args.apk_file = args.f
    logging.info('Auto-identified --apk-file.')
  elif args.f.endswith('.so') or '.' not in os.path.basename(args.f):
    logging.info('Auto-identified --elf-file.')
    args.elf_file = args.f
  elif args.f.endswith('.map') or args.f.endswith('.map.gz'):
    logging.info('Auto-identified --map-file.')
    args.map_file = args.f
  else:
    return False
  return True


def AddMainPathsArguments(parser):
  """Add arguments for _DeduceMainPaths()."""
  parser.add_argument('-f', metavar='FILE',
                      help='Auto-identify input file type.')
  parser.add_argument('--apk-file',
                      help='.apk file to measure. Other flags can generally be '
                           'derived when this is used.')
  parser.add_argument(
      '--resources-pathmap-file',
      help='.pathmap.txt file that contains a maping from '
      'original resource paths to shortened resource paths.')
  parser.add_argument('--minimal-apks-file',
                      help='.minimal.apks file to measure. Other flags can '
                           'generally be derived when this is used.')
  parser.add_argument('--mapping-file',
                      help='Proguard .mapping file for deobfuscation.')
  parser.add_argument('--elf-file',
                      help='Path to input ELF file. Currently used for '
                           'capturing metadata.')
  parser.add_argument('--map-file',
                      help='Path to input .map(.gz) file. Defaults to '
                           '{{elf_file}}.map(.gz)?. If given without '
                           '--elf-file, no size metadata will be recorded.')
  parser.add_argument('--no-source-paths', action='store_true',
                      help='Do not use .ninja files to map '
                           'object_path -> source_path')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('--tool-prefix',
                      help='Path prefix for c++filt, nm, readelf.')


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  parser.add_argument('--pak-file', action='append',
                      help='Paths to pak files.')
  parser.add_argument('--pak-info-file',
                      help='This file should contain all ids found in the pak '
                           'files that have been passed in.')
  parser.add_argument('--no-string-literals', dest='track_string_literals',
                      default=True, action='store_false',
                      help='Disable breaking down "** merge strings" into more '
                           'granular symbols.')
  parser.add_argument(
      '--relocations',
      action='store_true',
      help='Instead of counting binary size, count number of relative'
      'relocation instructions in ELF code.')
  parser.add_argument('--source-directory',
                      help='Custom path to the root source directory.')
  parser.add_argument(
      '--java-only', action='store_true', help='Run on only Java symbols')
  parser.add_argument(
      '--native-only', action='store_true', help='Run on only native symbols')
  parser.add_argument(
      '--no-java', action='store_true', help='Do not run on Java symbols')
  parser.add_argument(
      '--no-native', action='store_true', help='Do not run on native symbols')
  AddMainPathsArguments(parser)


def _DeduceMainPaths(args, parser, extracted_minimal_apk_path=None):
  """Computes main paths based on input, and deduces them if needed."""
  elf_path = args.elf_file
  map_path = args.map_file
  any_input = args.apk_file or args.minimal_apks_file or elf_path or map_path
  if not any_input:
    parser.error('Must pass at least one of --apk-file, --minimal-apks-file, '
                 '--elf-file, --map-file')
  output_directory_finder = path_util.OutputDirectoryFinder(
      value=args.output_directory,
      any_path_within_output_directory=any_input)

  aab_or_apk = args.apk_file or args.minimal_apks_file
  mapping_path = args.mapping_file
  resources_pathmap_path = args.resources_pathmap_file
  if aab_or_apk:
    # Allow either .minimal.apks or just .apks.
    aab_or_apk = aab_or_apk.replace('.minimal.apks', '.aab')
    aab_or_apk = aab_or_apk.replace('.apks', '.aab')
    if not mapping_path:
      mapping_path = aab_or_apk + '.mapping'
      logging.debug('Detected --mapping-file=%s', mapping_path)
    if not resources_pathmap_path:
      possible_pathmap_path = aab_or_apk + '.pathmap.txt'
      # This could be pointing to a stale pathmap file if path shortening was
      # previously enabled but is disabled for the current build. However, since
      # current apk/aab will have unshortened paths, looking those paths up in
      # the stale pathmap which is keyed by shortened paths would not find any
      # mapping and thus should not cause any issues.
      if os.path.exists(possible_pathmap_path):
        resources_pathmap_path = possible_pathmap_path
        logging.debug('Detected --resources-pathmap-file=%s',
                      resources_pathmap_path)

  apk_path = extracted_minimal_apk_path or args.apk_file
  apk_so_path = None
  if apk_path:
    with zipfile.ZipFile(apk_path) as z:
      lib_infos = [f for f in z.infolist()
                   if f.filename.endswith('.so') and f.file_size > 0]
    assert lib_infos, 'APK has no .so files.'
    # TODO(agrieve): Add support for multiple .so files, and take into account
    #     secondary architectures.
    apk_so_path = max(lib_infos, key=lambda x:x.file_size).filename
    logging.debug('Sub-apk path=%s', apk_so_path)
    if not elf_path and output_directory_finder.Tentative():
      elf_path = os.path.join(
          output_directory_finder.Tentative(), 'lib.unstripped',
          os.path.basename(apk_so_path.replace('crazy.', '')))
      logging.debug('Detected --elf-file=%s', elf_path)

  if map_path:
    if not map_path.endswith('.map') and not map_path.endswith('.map.gz'):
      parser.error('Expected --map-file to end with .map or .map.gz')
  elif elf_path:
    # Look for a .map file named for either the ELF file, or in the partitioned
    # native library case, the combined ELF file from which the main library was
    # extracted. Note that we don't yet have a tool_prefix to use here, but
    # that's not a problem for this use case.
    if _ElfIsMainPartition(elf_path, ''):
      map_path = elf_path.replace('.so', '__combined.so') + '.map'
    else:
      map_path = elf_path + '.map'
    if not os.path.exists(map_path):
      map_path += '.gz'
    if not os.path.exists(map_path):
      parser.error('Could not find .map(.gz)? file. Ensure you have built with '
                   'is_official_build=true and generate_linker_map=true, or '
                   'use --map-file to point me a linker map file.')

  tool_prefix = None
  if map_path:
    linker_name = _DetectLinkerName(map_path)
    logging.info('Linker name: %s' % linker_name)

    tool_prefix_finder = path_util.ToolPrefixFinder(
        value=args.tool_prefix,
        output_directory_finder=output_directory_finder,
        linker_name=linker_name)
    tool_prefix = tool_prefix_finder.Finalized()

  output_directory = None
  if not args.no_source_paths:
    output_directory = output_directory_finder.Finalized()

  size_info_prefix = None
  if output_directory and aab_or_apk:
    size_info_prefix = os.path.join(
        output_directory, 'size-info', os.path.basename(aab_or_apk))

  return (output_directory, tool_prefix, apk_path, mapping_path, apk_so_path,
          elf_path, map_path, resources_pathmap_path, linker_name,
          size_info_prefix)


def Run(args, parser):
  if not args.size_file.endswith('.size'):
    parser.error('size_file must end with .size')

  if args.f is not None:
    if not _AutoIdentifyInputFile(args):
      parser.error('Cannot identify file %s' % args.f)
  if args.apk_file and args.minimal_apks_file:
    parser.error('Cannot use both --apk-file and --minimal-apks-file.')

  if args.minimal_apks_file:
    # Can't use NamedTemporaryFile() because it uses atexit, which does
    # not play nice with fork().
    fd, extracted_minimal_apk_path = tempfile.mkstemp(suffix='.apk')
    try:
      logging.debug('Extracting %s', _APKS_MAIN_APK)
      with zipfile.ZipFile(args.minimal_apks_file) as z:
        os.write(fd, z.read(_APKS_MAIN_APK))
      os.close(fd)
      _RunInternal(args, parser, extracted_minimal_apk_path)
    finally:
      os.unlink(extracted_minimal_apk_path)
  else:
    _RunInternal(args, parser, None)


def _RunInternal(args, parser, extracted_minimal_apk_path):
  (output_directory, tool_prefix, apk_path, mapping_path, apk_so_path, elf_path,
   map_path, resources_pathmap_path,
   linker_name, size_info_prefix) = _DeduceMainPaths(
       args, parser, extracted_minimal_apk_path)

  knobs = SectionSizeKnobs(is_bundle=bool(extracted_minimal_apk_path))
  if args.source_directory:
    knobs.src_root = args.source_directory

  if args.java_only:
    knobs.analyze_java = True
    knobs.analyze_native = False
  if args.native_only:
    knobs.analyze_java = False
    knobs.analyze_native = True
  if args.no_java:
    knobs.analyze_java = False
  if args.no_native:
    knobs.analyze_native = False

  if args.relocations:
    knobs.relocations_mode = True
    knobs.analyze_java = False

  if not knobs.analyze_native:
    map_path = None
    elf_path = None
    apk_so_path = None

  metadata = CreateMetadata(map_path, elf_path, args.apk_file,
                            args.minimal_apks_file, tool_prefix,
                            output_directory, linker_name)
  section_sizes, raw_symbols = CreateSectionSizesAndSymbols(
      map_path=map_path,
      tool_prefix=tool_prefix,
      elf_path=elf_path,
      apk_path=apk_path,
      mapping_path=mapping_path,
      output_directory=output_directory,
      resources_pathmap_path=resources_pathmap_path,
      track_string_literals=args.track_string_literals,
      metadata=metadata,
      apk_so_path=apk_so_path,
      pak_files=args.pak_file,
      pak_info_file=args.pak_info_file,
      linker_name=linker_name,
      size_info_prefix=size_info_prefix,
      knobs=knobs)
  size_info = CreateSizeInfo(
      section_sizes, raw_symbols, metadata=metadata, normalize_names=False)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in describe.DescribeSizeInfoCoverage(size_info):
      logging.debug(line)
  logging.info('Recorded info for %d symbols', len(size_info.raw_symbols))
  logging.info('Recording metadata: \n  %s',
               '\n  '.join(describe.DescribeMetadata(size_info.metadata)))
  logging.info('Saving result to %s', args.size_file)
  file_format.SaveSizeInfo(size_info, args.size_file)
  size_in_mb = os.path.getsize(args.size_file) / 1024.0 / 1024.0
  logging.info('Done. File size is %.2fMiB.', size_in_mb)
