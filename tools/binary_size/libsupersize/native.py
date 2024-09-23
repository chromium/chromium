# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for creating native code symbols from ELF files."""

import calendar
import collections
import dataclasses
import datetime
import itertools
import logging
import os
import posixpath
import re
import subprocess
import sys
import tempfile

import ar
import archive_util
import demangle
import dwarfdump
import linker_map_parser
import models
import ninja_parser
import nm
import obj_analyzer
import parallel
import path_util
import readelf
import string_extract
import zip_util

# When ensuring matching section sizes between .elf and .map files, these
# sections should be ignored. When lld creates a combined library with
# partitions, some sections (like .text) exist in each partition, but the ones
# below are common. At library splitting time, llvm-objcopy pulls what's needed
# from these sections into the new libraries. Hence, the ELF sections will end
# up smaller than the combined .map file sections.
_SECTION_SIZE_BLOCKLIST = ['.symtab', '.shstrtab', '.strtab']

# A limit on the number of symbols an address can have, before these symbols
# are compacted into shared symbols. Increasing this value causes more data
# to be stored .size files, but is also more expensive.
# Effect as of Oct 2017, with min_pss = max:
# 1: shared .text syms = 1772874 bytes, file size = 9.43MiB (645476 syms).
# 2: shared .text syms = 1065654 bytes, file size = 9.58MiB (669952 syms).
# 6: shared .text syms = 464058 bytes, file size = 10.11MiB (782693 syms).
# 10: shared .text syms = 365648 bytes, file size = 10.24MiB (813758 syms).
# 20: shared .text syms = 86202 bytes, file size = 10.38MiB (854548 syms).
# 40: shared .text syms = 48424 bytes, file size = 10.50MiB (890396 syms).
# 50: shared .text syms = 41860 bytes, file size = 10.54MiB (902304 syms).
# max: shared .text syms = 0 bytes, file size = 11.10MiB (1235449 syms).
_MAX_SAME_NAME_ALIAS_COUNT = 40  # 50kb is basically negligible.


# Holds computation state that is live only when an output directory exists.
@dataclasses.dataclass
class _OutputDirectoryContext:
  elf_object_paths: list  # Non-None only when elf_path is.
  known_inputs: list  # Non-None only when elf_path is.
  output_directory: str
  thin_archives: list


@dataclasses.dataclass
class ElfInfo:
  architecture: str  # Results of ArchFromElf().
  build_id: str  # Result of BuildIdFromElf().
  section_ranges: dict  # Results of SectionInfoFromElf().
  size: int  # Result of os.path.getsize().

  def OverheadSize(self):
    section_sizes_total_without_bss = sum(
        size for k, (_, size) in self.section_ranges.items()
        if k not in models.BSS_SECTIONS)
    ret = self.size - section_sizes_total_without_bss
    assert ret >= 0, 'Negative ELF overhead {}'.format(ret)
    return ret


def _CreateElfInfo(elf_path):
  return ElfInfo(architecture=readelf.ArchFromElf(elf_path),
                 build_id=readelf.BuildIdFromElf(elf_path),
                 section_ranges=readelf.SectionInfoFromElf(elf_path),
                 size=os.path.getsize(elf_path))


def _AddSourcePathsUsingObjectPaths(ninja_source_mapper, raw_symbols):
  logging.info('Looking up source paths from ninja files')
  for symbol in raw_symbols:
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
  logging.debug('Looking up source paths from dwarfdump')
  query_count = 0
  match_count = 0
  for symbol in raw_symbols:
    if symbol.section_name != models.SECTION_TEXT:
      continue
    query_count += 1
    source_path = dwarf_source_mapper.FindSourceForTextAddress(symbol.address)
    if source_path:
      match_count += 1
      symbol.source_path = source_path
  logging.info('dwarfdump found paths for %d of %d .text symbols.', match_count,
               query_count)
  # Majority of unmatched queries are for assembly source files (ex libav1d)
  # and v8 builtins.
  if query_count > 0:
    unmatched_ratio = (query_count - match_count) / query_count
    assert unmatched_ratio < 0.2, (
        'Percentage of failing |dwarf_source_mapper| queries ' +
        '({}%) >= 20% '.format(unmatched_ratio * 100) +
        'FindSourceForTextAddress() likely has a bug.')


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
    if (symbol.IsStringLiteral() or not full_name or full_name[0] in '*.'
        or  # e.g. ** merge symbols, .Lswitch.table
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
        new_sym = models.Symbol(symbol.section_name,
                                symbol.size,
                                address=symbol.address,
                                full_name=full_name,
                                object_path=object_path,
                                aliases=aliases)
        aliases.append(new_sym)
        ret.append(new_sym)

  logging.debug(
      'Cross-referenced %d symbols with nm output. '
      'num_unknown_names=%d num_path_mismatches=%d '
      'num_aliases_created=%d', num_found_paths, num_unknown_names,
      num_path_mismatches, num_aliases_created)
  # Currently: num_unknown_names=1246 out of 591206 (0.2%).
  if num_unknown_names > min(20, len(raw_symbols) * 0.01):
    logging.warning(
        'Abnormal number of symbols not found in .o files (%d of %d)',
        num_unknown_names, len(raw_symbols))
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
        symbol = models.Symbol(models.SECTION_RODATA,
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
      if (prev_symbol.address == symbol.address
          and prev_symbol.size == symbol.size):
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


def _AddOutlinedSymbolCountsFromNm(raw_symbols, names_by_address):
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
                       (name, address, ','.join('%x' % a
                                                for a in missing_names[name])))

  is_small_file = len(raw_symbols) < 1000
  if not is_small_file and num_new_symbols / len(raw_symbols) < .05:
    logging.warning(
        'Number of aliases is oddly low (%.0f%%). It should '
        'usually be around 25%%.', num_new_symbols / len(raw_symbols) * 100)

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
      # CompactLargeAliasesIntoSharedSymbols(), which assumes aliases differ
      # only by path. The field will be set afterwards by _ConnectNmAliases().
      new_syms.append(
          models.Symbol(sym.section_name,
                        sym.size,
                        address=sym.address,
                        full_name=full_name))
    ret += new_syms
  ret += raw_symbols[prev_src:]
  assert expected_num_symbols == len(ret)
  return ret


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


def _ParseElfInfo(native_spec, outdir_context=None):
  """Adds ELF section ranges and symbols."""
  assert native_spec.map_path or native_spec.elf_path, (
      'Need a linker map or an ELF file.')
  assert native_spec.map_path or not native_spec.track_string_literals, (
      'track_string_literals not yet implemented without map file')
  if native_spec.elf_path:
    elf_section_ranges = readelf.SectionInfoFromElf(native_spec.elf_path)

    # Run nm on the elf file to retrieve the list of symbol names per-address.
    # This list is required because the .map file contains only a single name
    # for each address, yet multiple symbols are often coalesced when they are
    # identical. This coalescing happens mainly for small symbols and for C++
    # templates. Such symbols make up ~500kb of libchrome.so on Android.
    elf_nm_result = nm.CollectAliasesByAddressAsync(native_spec.elf_path)

    # Run nm on all .o/.a files to retrieve the symbol names within them.
    # The list is used to detect when multiple .o files contain the same symbol
    # (e.g. inline functions), and to update the object_path / source_path
    # fields accordingly.
    # Looking in object files is required because the .map file choses a
    # single path for these symbols.
    # Rather than record all paths for each symbol, set the paths to be the
    # common ancestor of all paths.
    if outdir_context and native_spec.map_path:
      bulk_analyzer = obj_analyzer.BulkObjectFileAnalyzer(
          outdir_context.output_directory,
          track_string_literals=native_spec.track_string_literals)
      bulk_analyzer.AnalyzePaths(outdir_context.elf_object_paths)

  if native_spec.map_path:
    logging.info('Parsing Linker Map')
    map_section_ranges, raw_symbols, linker_map_extras = (
        linker_map_parser.ParseFile(native_spec.map_path))

    if outdir_context and outdir_context.thin_archives:
      _ResolveThinArchivePaths(raw_symbols, outdir_context.thin_archives)
  else:
    logging.info('Collecting symbols from nm')
    raw_symbols = nm.CreateUniqueSymbols(native_spec.elf_path,
                                         elf_section_ranges)

  if native_spec.elf_path and native_spec.map_path:
    logging.debug('Validating section sizes')
    differing_elf_section_sizes = {}
    differing_map_section_sizes = {}
    for k, (_, elf_size) in elf_section_ranges.items():
      if k in _SECTION_SIZE_BLOCKLIST:
        continue
      _, map_size = map_section_ranges.get(k)
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
      merge_string_syms = [
          s for s in raw_symbols if s.full_name == '** merge strings'
          or s.full_name == '** lld merge strings'
      ]
      # More likely for there to be a bug in supersize than an ELF to not have a
      # single string literal.
      assert merge_string_syms
      string_ranges = [(s.address, s.size) for s in merge_string_syms]
      bulk_analyzer.AnalyzeStringLiterals(native_spec.elf_path, string_ranges)

  # Map file for some reason doesn't demangle all names.
  # Demangle prints its own log statement.
  demangle.DemangleRemainingSymbols(raw_symbols)

  object_paths_by_name = {}
  if native_spec.elf_path:
    logging.info(
        'Adding symbols removed by identical code folding (as reported by nm)')
    # This normally does not block (it's finished by this time).
    names_by_address = elf_nm_result.get()
    if native_spec.map_path:
      # This rewrites outlined symbols from |map_path|, and can be skipped if
      # symbols already came from nm (e.g., for dwarf mode).
      _AddOutlinedSymbolCountsFromNm(raw_symbols, names_by_address)

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
        replacements = _CreateMergeStringsReplacements(
            merge_string_syms, list_of_positions_by_object_path)
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
    sym_and_string_literals = string_extract.ReadStringLiterals(
        raw_symbols, native_spec.elf_path)
    for sym, data in sym_and_string_literals:
      sym.full_name = string_extract.GetNameOfStringLiteralBytes(data)

  # If we have an ELF file, use its ranges as the source of truth, since some
  # sections can differ from the .map.
  return (elf_section_ranges if native_spec.elf_path else map_section_ranges,
          raw_symbols, object_paths_by_name)


def _AddUnattributedSectionSymbols(raw_symbols, section_ranges, source_path):
  # Create symbols for ELF sections not covered by existing symbols.
  logging.info('Searching for symbol gaps...')
  new_syms_by_section = collections.defaultdict(list)
  seen_sections = set()

  for section_name, group in itertools.groupby(
      raw_symbols, lambda s: s.section_name):
    seen_sections.add(section_name)
    # Get last Last symbol in group.
    sym = None  # Needed for pylint.
    for sym in group:
      pass
    end_address = sym.end_address  # pylint: disable=undefined-loop-variable
    size_from_syms = end_address - section_ranges[section_name][0]
    overhead = section_ranges[section_name][1] - size_from_syms
    assert overhead >= 0, (
        'Last symbol (%s) ends %d bytes after section boundary (%x)' %
        (sym, -overhead, sum(section_ranges[section_name])))
    if overhead > 0 and section_name not in models.BSS_SECTIONS:
      new_syms_by_section[section_name].append(
          models.Symbol(section_name,
                        overhead,
                        address=end_address,
                        full_name='** {} (unattributed)'.format(section_name),
                        source_path=source_path))
      logging.info('Last symbol in %s does not reach end of section, gap=%d',
                   section_name, overhead)

  # Sections that should not bundle into ".other".
  unsummed_sections, summed_sections = models.ClassifySections(
      section_ranges.keys())
  ret = []
  other_symbols = []
  # Sort keys to ensure consistent order (> 1 sections may have address = 0).
  for section_name, (_, section_size) in list(section_ranges.items()):
    if section_name in seen_sections:
      continue
    # Handle sections that don't appear in |raw_symbols|.
    if (section_name not in unsummed_sections
        and section_name not in summed_sections):
      other_symbols.append(
          models.Symbol(models.SECTION_OTHER,
                        section_size,
                        full_name='** ELF Section: {}'.format(section_name),
                        source_path=source_path))
      archive_util.ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                                      section_size)
    else:
      ret.append(
          models.Symbol(section_name,
                        section_size,
                        full_name='** ELF Section: {}'.format(section_name),
                        source_path=source_path))
  other_symbols.sort(key=lambda s: (s.address, s.full_name))

  # TODO(agrieve): It would probably simplify things to use a dict of
  #     section_name->raw_symbols while creating symbols.
  # Merge |new_syms_by_section| into |raw_symbols| while maintaining ordering.
  for section_name, group in itertools.groupby(
      raw_symbols, lambda s: s.section_name):
    ret.extend(group)
    ret.extend(new_syms_by_section[section_name])
  return ret, other_symbols


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


def _ElfInfoFromApk(apk_path, apk_so_path):
  with zip_util.UnzipToTemp(apk_path, apk_so_path) as temp:
    return _CreateElfInfo(temp)


def _CountRelocationsFromElf(elf_path):
  args = [path_util.GetReadElfPath(), '-r', elf_path]
  stdout = subprocess.check_output(args).decode('ascii')
  relocations = re.findall(
      'Relocation section .* at offset .* contains (\d+) entries', stdout)
  return sum([int(i) for i in relocations])


def _FindToolchainSubdirs(output_directory):
  return [
      n for n in os.listdir(output_directory)
      if os.path.exists(os.path.join(output_directory, n, 'toolchain.ninja'))
  ]


def CreateMetadata(*, native_spec, elf_info, shorten_path):
  """Returns metadata for the given native_spec / elf_info."""
  logging.debug('Constructing native metadata')
  native_metadata = {}
  native_metadata[models.METADATA_ELF_ALGORITHM] = native_spec.algorithm

  if elf_info:
    native_metadata[models.METADATA_ELF_ARCHITECTURE] = elf_info.architecture
    native_metadata[models.METADATA_ELF_BUILD_ID] = elf_info.build_id

  if native_spec.apk_so_path:
    native_metadata[models.METADATA_ELF_APK_PATH] = native_spec.apk_so_path

  if native_spec.elf_path:
    native_metadata[models.METADATA_ELF_FILENAME] = shorten_path(
        native_spec.elf_path)
    timestamp_obj = datetime.datetime.utcfromtimestamp(
        os.path.getmtime(native_spec.elf_path))
    timestamp = calendar.timegm(timestamp_obj.timetuple())
    native_metadata[models.METADATA_ELF_MTIME] = timestamp

  if native_spec.map_path:
    native_metadata[models.METADATA_MAP_FILENAME] = shorten_path(
        native_spec.map_path)
  return native_metadata


def CreateSymbols(*,
                  apk_spec,
                  native_spec,
                  output_directory=None,
                  pak_id_map=None):
  """Creates native symbols for the given native_spec.

  Args:
    apk_spec: Instance of ApkSpec, or None.
    native_spec: Instance of NativeSpec.
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    pak_id_map: Instance of PakIdMap.

  Returns:
    A tuple of (section_ranges, raw_symbols, elf_info, metrics_by_file), where
    metrics_by_file is a dict from file name to a dict of {metric_name: value}.
  """
  apk_elf_info_result = None
  if apk_spec and native_spec.apk_so_path:
    # Extraction takes around 1 second, so do it in parallel.
    apk_elf_info_result = parallel.ForkAndCall(
        _ElfInfoFromApk, (apk_spec.apk_path, native_spec.apk_so_path))

  raw_symbols = []
  ninja_source_mapper = None
  dwarf_source_mapper = None
  section_ranges = {}
  ninja_elf_object_paths = None
  metrics_by_file = {}
  if output_directory and native_spec.map_path:
    # Finds all objects passed to the linker and creates a map of .o -> .cc.
    ninja_source_mapper, ninja_elf_object_paths = _ParseNinjaFiles(
        output_directory, native_spec.elf_path)
  elif native_spec.elf_path:
    logging.info('Parsing source path info via dwarfdump')
    dwarf_source_mapper = dwarfdump.CreateAddressSourceMapper(
        native_spec.elf_path)
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

  if output_directory:
    toolchain_subdirs = _FindToolchainSubdirs(output_directory)
    outdir_context = _OutputDirectoryContext(elf_object_paths=elf_object_paths,
                                             known_inputs=known_inputs,
                                             output_directory=output_directory,
                                             thin_archives=thin_archives)
  else:
    toolchain_subdirs = None
    outdir_context = None

  object_paths_by_name = None
  if native_spec.elf_path or native_spec.map_path:
    section_ranges, raw_symbols, object_paths_by_name = _ParseElfInfo(
        native_spec, outdir_context=outdir_context)
    if pak_id_map and native_spec.map_path:
      # For trichrome, pak files are in different apks than native library,
      # so need to pass along pak_id_map separately and ensure
      # TrichromeLibrary appears first in .ssargs file.
      logging.debug('Extracting pak IDs from symbol names')
      pak_id_map.Update(object_paths_by_name, ninja_source_mapper)

  elf_info = None
  if apk_elf_info_result:
    logging.debug('Extracting section sizes from .so within .apk')
    elf_info = apk_elf_info_result.get()
    if native_spec.elf_path:
      expected_build_id = readelf.BuildIdFromElf(native_spec.elf_path)
      assert elf_info.build_id == expected_build_id, (
          'BuildID of {} != $APK/{}: {} != {}'.format(native_spec.elf_path,
                                                      native_spec.apk_so_path,
                                                      expected_build_id,
                                                      elf_info.build_id))
  elif native_spec.elf_path:
    # Strip ELF before capturing section information to avoid recording
    # debug sections.
    with tempfile.NamedTemporaryFile(
        suffix=os.path.basename(native_spec.elf_path)) as f:
      strip_path = path_util.GetStripPath()
      subprocess.run([strip_path, '-o', f.name, native_spec.elf_path],
                     check=True)
      elf_info = _CreateElfInfo(f.name)

  if elf_info:
    section_ranges = elf_info.section_ranges.copy()
    if native_spec.elf_path:
      key = posixpath.basename(native_spec.elf_path)
      metrics_by_file[key] = {
          f'{models.METRICS_SIZE}/{k}': size
          for (k, (offset, size)) in section_ranges.items()
      }
      relocations_count = _CountRelocationsFromElf(native_spec.elf_path)
      metrics_by_file[key][
          f'{models.METRICS_COUNT}/{models.METRICS_COUNT_RELOCATIONS}'] = (
              relocations_count)

  source_path = ''
  if native_spec.apk_so_path:
    # Put section symbols under $NATIVE/libfoo.so (abi)/...
    source_path = '{}/{} ({})'.format(
        models.NATIVE_PREFIX_PATH, posixpath.basename(native_spec.apk_so_path),
        elf_info.architecture)

  raw_symbols, other_symbols = _AddUnattributedSectionSymbols(
      raw_symbols, section_ranges, source_path)

  if elf_info:
    elf_overhead_size = elf_info.OverheadSize()
    elf_overhead_symbol = models.Symbol(models.SECTION_OTHER,
                                        elf_overhead_size,
                                        full_name='Overhead: ELF file',
                                        source_path=source_path)
    archive_util.ExtendSectionRange(section_ranges, models.SECTION_OTHER,
                                    elf_overhead_size)
    other_symbols.append(elf_overhead_symbol)

  # Always have .other come last.
  other_symbols.sort(key=lambda s: (s.IsOverhead(), s.full_name.startswith(
      '**'), s.address, s.full_name))

  if ninja_source_mapper:
    _AddSourcePathsUsingObjectPaths(ninja_source_mapper, raw_symbols)
  elif dwarf_source_mapper:
    _AddSourcePathsUsingAddress(dwarf_source_mapper, raw_symbols)

  raw_symbols.extend(other_symbols)

  # Path normalization must come before compacting aliases so that
  # ancestor paths do not mix generated and non-generated paths.
  archive_util.NormalizePaths(raw_symbols,
                              gen_dir_regex=native_spec.gen_dir_regex,
                              toolchain_subdirs=toolchain_subdirs)

  if native_spec.elf_path or native_spec.map_path:
    logging.info('Converting excessive aliases into shared-path symbols')
    archive_util.CompactLargeAliasesIntoSharedSymbols(
        raw_symbols, _MAX_SAME_NAME_ALIAS_COUNT)

    logging.debug('Connecting nm aliases')
    _ConnectNmAliases(raw_symbols)

  return section_ranges, raw_symbols, elf_info, metrics_by_file
