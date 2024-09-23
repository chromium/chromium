#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parser for linker map files.

The format of a linker map file depends on the linker that generates it. This
file uses "coded linker name" to identify formats and variants:

  'gold': The gold linker (usage is being deprecated by Chrome).
  'lld_v0': LLD linker (no LTO), old format.
  'lld-lto_v0': LLD linker with ThinLTO, old format.
  'lld_v1': LLD linker (no LTO), new format.
  'lld-lto_v1': LLD linker with ThinLTO, new format.
"""

import argparse
import code
import collections
import gzip
import itertools
import logging
import os
import re
import readline
import sys

import demangle
import models

# About linker maps:
# * "Discarded input sections" include symbols merged with other symbols
#   (aliases), so the information there is not actually a list of unused things.
# * Linker maps include symbols that do not have names (with object path),
#   whereas "nm" skips over these (they don't account for much though).
# * The parse time for compressed linker maps is dominated by ungzipping.

_STRIP_NAME_PREFIX = {
    models.FLAG_STARTUP: 8,
    models.FLAG_UNLIKELY: 9,
    models.FLAG_REL_LOCAL: 10,
    models.FLAG_REL: 4,
    models.FLAG_HOT: 4,
}


def _OpenMaybeGzAsText(path):
  """Calls `gzip.open()` if |path| ends in ".gz", otherwise calls `open()`."""
  if path.endswith('.gz'):
    return gzip.open(path, 'rt')
  return open(path, 'rt')


def _FlagsFromMangledName(name):
  # Currently, lld map files have section = '.text.startup' and put the symbol
  # name in the section break-down ("level 3 symbols").
  if name.startswith('startup.') or name == 'startup':
    return models.FLAG_STARTUP
  if name.startswith('unlikely.'):
    return models.FLAG_UNLIKELY
  if name.startswith('rel.local.'):
    return models.FLAG_REL_LOCAL
  if name.startswith('rel.'):
    return models.FLAG_REL
  if name.startswith('hot.'):
    return models.FLAG_HOT
  return 0


def _NormalizeName(name):
  # Outlined functions have names like OUTLINED_FUNCTION_0, which can
  # appear 1000+ time, and can cause false aliasing. We treat these as
  # special cases by designating them as a placeholder symbols and
  # renaming them to '** outlined function'.
  if name.startswith('OUTLINED_FUNCTION_'):
    return '** outlined function'
  if name.startswith('.L.str'):
    return models.STRING_LITERAL_NAME
  if name.endswith(' (.cfi)'):
    return name[:-7]
  return name


class MapFileParserGold:
  """Parses a linker map file from gold linker."""
  # Map file writer for gold linker:
  # https://github.com/gittup/binutils/blob/HEAD/gold/mapfile.cc

  def __init__(self):
    self._common_symbols = []
    self._symbols = []
    self._section_ranges = {}
    self._lines = None

  def Parse(self, lines):
    """Parses a linker map file.

    Args:
      lines: Iterable of lines, the first of which has been consumed to
      identify file type.

    Returns:
      A tuple of (section_ranges, symbols, extras).
    """
    self._lines = iter(lines)
    logging.debug('Scanning for Header')

    while True:
      line = self._SkipToLineWithPrefix('Common symbol', 'Memory map')
      if line.startswith('Common symbol'):
        self._common_symbols = self._ParseCommonSymbols()
        logging.debug('.bss common entries: %d', len(self._common_symbols))
        continue
      if line.startswith('Memory map'):
        self._ParseSections()
      break
    return self._section_ranges, self._symbols, {}

  def _SkipToLineWithPrefix(self, prefix, prefix2=None):
    for l in self._lines:
      if l.startswith(prefix) or (prefix2 and l.startswith(prefix2)):
        return l
    return None

  def _ParsePossiblyWrappedParts(self, line, count):
    parts = line.split(None, count - 1)
    if not parts:
      return None
    if len(parts) != count:
      line = next(self._lines)
      parts.extend(line.split(None, count - len(parts) - 1))
      assert len(parts) == count, 'parts: ' + ' '.join(parts)
    parts[-1] = parts[-1].rstrip()
    return parts

  def _ParseCommonSymbols(self):
    # Common symbol       size              file
    #
    # ff_cos_131072       0x40000           obj/third_party/<snip>
    # ff_cos_131072_fixed
    #                     0x20000           obj/third_party/<snip>
    ret = []
    next(self._lines)  # Skip past blank line

    name, size_str, path = None, None, None
    for l in self._lines:
      parts = self._ParsePossiblyWrappedParts(l, 3)
      if not parts:
        break
      name, size_str, path = parts
      sym = models.Symbol(
          models.SECTION_BSS,
          int(size_str[2:], 16),
          full_name=name,
          object_path=path)
      ret.append(sym)
    return ret

  def _ParseSections(self):
    # .text           0x0028c600  0x22d3468
    #  .text.startup._GLOBAL__sub_I_bbr_sender.cc
    #                 0x0028c600       0x38 obj/net/net/bbr_sender.o
    #  .text._reset   0x00339d00       0xf0 obj/third_party/icu/icuuc/ucnv.o
    #  ** fill        0x0255fb00   0x02
    #  .text._ZN4base8AutoLockD2Ev
    #                 0x00290710        0xe obj/net/net/file_name.o
    #                 0x00290711                base::AutoLock::~AutoLock()
    #                 0x00290711                base::AutoLock::~AutoLock()
    # .text._ZNK5blink15LayoutBlockFlow31mustSeparateMarginAfterForChildERK...
    #                 0xffffffffffffffff       0x46 obj/...
    #                 0x006808e1                blink::LayoutBlockFlow::...
    # .text.OUTLINED_FUNCTION_0
    #                 0x002a2000       0x20 obj/net/net/tag.o
    # .bss
    #  .bss._ZGVZN11GrProcessor11initClassIDI10LightingFPEEvvE8kClassID
    #                 0x02d4b294        0x4 obj/skia/skia/SkLightingShader.o
    #                 0x02d4b294   guard variable for void GrProcessor::ini...
    # .data           0x0028c600  0x22d3468
    #  .data.rel.ro._ZTVN3gvr7android19ScopedJavaGlobalRefIP12_jfloatArrayEE
    #                 0x02d1e668       0x10 ../third_party/.../libfoo.a(bar.o)
    #                 0x02d1e668   vtable for gvr::android::GlobalRef<_jflo...
    #  ** merge strings
    #                 0x0255fb00   0x1f2424
    #  ** merge constants
    #                 0x0255fb00   0x8
    # ** common       0x02db5700   0x13ab48
    syms = self._symbols
    while True:
      line = self._SkipToLineWithPrefix('.')
      if not line:
        break
      section_name = None
      try:
        # Parse section name and size.
        parts = self._ParsePossiblyWrappedParts(line, 3)
        if not parts:
          break
        section_name, section_address_str, section_size_str = parts
        section_address = int(section_address_str[2:], 16)
        section_size = int(section_size_str[2:], 16)
        self._section_ranges[section_name] = (section_address, section_size)
        if (section_name in models.BSS_SECTIONS
            or section_name in (models.SECTION_RODATA, models.SECTION_TEXT)
            or section_name.startswith(models.SECTION_DATA)):
          logging.info('Parsing %s', section_name)
          if section_name in models.BSS_SECTIONS:
            # Common symbols have no address.
            syms.extend(self._common_symbols)
          prefix_len = len(section_name) + 1  # + 1 for the trailing .
          symbol_gap_count = 0
          merge_symbol_start_address = section_address
          sym_count_at_start = len(syms)
          line = next(self._lines)
          # Parse section symbols.
          while True:
            if not line or line.isspace():
              break
            if line.startswith(' **'):
              zero_index = line.find('0')
              if zero_index == -1:
                # Line wraps.
                name = line.strip()
                line = next(self._lines)
              else:
                # Line does not wrap.
                name = line[:zero_index].strip()
                line = line[zero_index:]
              address_str, size_str = self._ParsePossiblyWrappedParts(line, 2)
              line = next(self._lines)
              # These bytes are already accounted for.
              if name == '** common':
                continue
              address = int(address_str[2:], 16)
              size = int(size_str[2:], 16)
              path = None
              sym = models.Symbol(section_name, size, address=address,
                                  full_name=name, object_path=path)
              syms.append(sym)
              if merge_symbol_start_address > 0:
                merge_symbol_start_address += size
            else:
              # A normal symbol entry.
              subsection_name, address_str, size_str, path = (
                  self._ParsePossiblyWrappedParts(line, 4))
              size = int(size_str[2:], 16)
              assert subsection_name.startswith(section_name), (
                  'subsection name was: ' + subsection_name)
              mangled_name = subsection_name[prefix_len:]
              name = None
              address_str2 = None
              while True:
                line = next(self._lines).rstrip()
                if not line or line.startswith(' .'):
                  break
                # clang includes ** fill, but gcc does not.
                if line.startswith(' ** fill'):
                  # Alignment explicitly recorded in map file. Rather than
                  # record padding based on these entries, we calculate it
                  # using addresses. We do this because fill lines are not
                  # present when compiling with gcc (only for clang).
                  continue
                if line.startswith(' **'):
                  break
                if name is None:
                  address_str2, name = self._ParsePossiblyWrappedParts(line, 2)

              if address_str == '0xffffffffffffffff':
                # The section needs special handling (e.g., a merge section)
                # It also generally has a large offset after it, so don't
                # penalize the subsequent symbol for this gap (e.g. a 50kb gap).
                # There seems to be no corelation between where these gaps occur
                # and the symbols they come in-between.
                # TODO(agrieve): Learn more about why this happens.
                if address_str2:
                  address = int(address_str2[2:], 16) - 1
                elif syms and syms[-1].address > 0:
                  # Merge sym with no second line showing real address.
                  address = syms[-1].end_address
                else:
                  logging.warning('First symbol of section had address -1')
                  address = 0

                merge_symbol_start_address = address + size
              else:
                address = int(address_str[2:], 16)
                # Finish off active address gap / merge section.
                if merge_symbol_start_address:
                  merge_size = address - merge_symbol_start_address
                  merge_symbol_start_address = 0
                  if merge_size > 0:
                    # merge_size == 0 for the initial symbol generally.
                    logging.debug('Merge symbol of size %d found at:\n  %r',
                                  merge_size, syms[-1])
                    # Set size=0 so that it will show up as padding.
                    sym = models.Symbol(
                        section_name, 0,
                        address=address,
                        full_name='** symbol gap %d' % symbol_gap_count)
                    symbol_gap_count += 1
                    syms.append(sym)

              #  .text.res_findResource_60
              #                 0x00178de8       0x12a obj/...
              #                 0x00178de9                res_findResource_60
              #  .text._ZN3url6ParsedC2Ev
              #                 0x0021ad62       0x2e obj/url/url/url_parse.o
              #                 0x0021ad63                url::Parsed::Parsed()
              #  .text.unlikely._ZN4base3CPUC2Ev
              #                 0x003f9d3c       0x48 obj/base/base/cpu.o
              #                 0x003f9d3d                base::CPU::CPU()
              full_name = name or mangled_name
              if mangled_name and (not name or mangled_name.startswith('_Z') or
                                   '._Z' in mangled_name):
                full_name = mangled_name

              flags = _FlagsFromMangledName(mangled_name)
              if full_name:
                if flags:
                  full_name = full_name[_STRIP_NAME_PREFIX[flags]:]
                else:
                  full_name = _NormalizeName(full_name)

              sym = models.Symbol(section_name, size, address=address,
                                  full_name=full_name, object_path=path,
                                  flags=flags)
              syms.append(sym)
          logging.debug('Symbol count for %s: %d', section_name,
                        len(syms) - sym_count_at_start)
      except:
        logging.error('Problem line: %r', line)
        logging.error('In section: %r', section_name)
        raise


class MapFileParserLld:
  """Parses a linker map file from LLD."""
  # Map file writer for LLD linker (for ELF):
  # https://github.com/llvm-mirror/lld/blob/HEAD/ELF/MapFile.cpp
  _LINE_RE_V0 = re.compile(r'([0-9a-f]+)\s+([0-9a-f]+)\s+(\d+) ( *)(.*)')
  _LINE_RE_V1 = re.compile(
      r'\s*[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\d+) ( *)(.*)')
  _LINE_RE = [_LINE_RE_V0, _LINE_RE_V1]

  def __init__(self, linker_name):
    self._linker_name = linker_name
    self._common_symbols = []
    self._section_ranges = {}

  @staticmethod
  def ParseArmAnnotations(tok):
    """Decides whether a Level 3 token is an annotation.

    Returns:
      A 2-tuple (is_annotation, next_thumb2_mode):
        is_annotation: Whether |tok| is an annotation.
        next_thumb2_mode: New |thumb2_mode| value, or None if keep old value.
    """
    # Annotations for ARM match '$t', '$d.1', but not '$_21::invoke'.
    if tok.startswith('$') and (len(tok) == 2 or
                                (len(tok) >= 3 and tok[2] == '.')):
      if tok.startswith('$t'):
        return True, True  # Is annotation, enter Thumb2 mode.
      if tok.startswith('$a'):
        return True, False  # Is annotation, enter ARM32 mode.
      return True, None  # Is annotation, keep old |thumb2_mode| value.
    return False, None  # Not annotation, keep old |thumb2_mode| value.

  def Tokenize(self, lines):
    """Generator to filter and tokenize linker map lines."""
    # Extract e.g., 'lld_v0' -> 0, or 'lld-lto_v1' -> 1.
    map_file_version = int(self._linker_name.split('_v')[1])
    pattern = MapFileParserLld._LINE_RE[map_file_version]

    # A Level 3 symbol can have |size == 0| in some situations (e.g., assembly
    # code symbols). To provided better size estimates in this case, the "span"
    # of a Level 3 symbol is computed as:
    #  (A) The |address| difference compared to the next Level 3 symbol.
    #  (B) If the Level 3 symbol is the last among Level 3 lines nested under a
    #      Level 2 line: The difference between the Level 3 symbol's |address|
    #      and the containing Level 2 line's end address.
    # To handle (A), |lines| is visited using a one-step lookahead, using
    # |sentinel| to handle the last line. To handle (B), |level2_end_address| is
    # computed for each Level 2 line.
    sentinel = '0 0 0 0 THE_END'
    assert pattern.match(sentinel)
    level2_end_address = None
    thumb2_mode = False
    (line, address, size, level, tok) = (None, None, None, None, None)
    for next_line in itertools.chain(lines, (sentinel,)):
      m = pattern.match(next_line)
      if m is None:
        continue
      next_address = int(m.group(1), 16)
      next_size = int(m.group(2), 16)
      next_level = (len(m.group(4)) // 8) + 1  # Add 1 to agree with comments.
      next_tok = m.group(5)

      if next_level == 3:
        assert level >= 2, 'Cannot jump from Level 1 to Level 3.'
        # Detect annotations. If found, maybe update |thumb2_mode|, then skip.
        (is_annotation, next_thumb2_mode) = (
            MapFileParserLld.ParseArmAnnotations(next_tok))
        if is_annotation:
          if next_thumb2_mode:
            thumb2_mode = next_thumb2_mode
          continue  # Skip annotations.
        if thumb2_mode:
          # Adjust odd address to even. Alignment is not guanteed for all
          # symbols (e.g., data, or x86), so this is judiciously applied.
          next_address &= ~1
      else:
        thumb2_mode = False  # Resets on leaving Level 3.

      if address is not None:
        span = None
        if level == 3:
          span = next_address if next_level == 3 else level2_end_address
          span -= address
        elif level == 2:
          level2_end_address = address + size
        yield (line, address, size, level, span, tok)

      line = next_line
      address = next_address
      size = next_size
      level = next_level
      tok = next_tok

  def Parse(self, lines):
    """Parses a linker map file.

    Args:
      lines: Iterable of lines, the first of which has been consumed to
      identify file type.

    Returns:
      A tuple of (section_ranges, symbols).
    """
    # Newest format:
    #     VMA      LMA     Size Align Out     In      Symbol
    #     194      194       13     1 .interp
    #     194      194       13     1         <internal>:(.interp)
    #     1a8      1a8     22d8     4 .ARM.exidx
    #     1b0      1b0        8     4         obj/sandbox/syscall.o:(.ARM.exidx)
    #     400      400   123400    64 .text
    #     600      600       14     4         ...:(.text.OUTLINED_FUNCTION_0)
    #     600      600        0     1                 $x.3
    #     600      600       14     1                 OUTLINED_FUNCTION_0
    #  123800   123800    20000   256 .rodata
    #  123800   123800       4      4         ...:o:(.rodata._ZN3fooE.llvm.1234)
    #  123800   123800       4      1                 foo (.llvm.1234)
    #  123804   123804       4      4         ...:o:(.rodata.bar.llvm.1234)
    #  123804   123804       4      1                 bar.llvm.1234
    # Older format:
    # Address          Size             Align Out     In      Symbol
    # 00000000002002a8 000000000000001c     1 .interp
    # 00000000002002a8 000000000000001c     1         <internal>:(.interp)
    # ...
    # 0000000000201000 0000000000000202    16 .text
    # 0000000000201000 000000000000002a     1         /[...]/crt1.o:(.text)
    # 0000000000201000 0000000000000000     0                 _start
    # 000000000020102a 0000000000000000     1         /[...]/crti.o:(.text)
    # 0000000000201030 00000000000000bd    16         /[...]/crtbegin.o:(.text)
    # 0000000000201030 0000000000000000     0             deregister_tm_clones
    # 0000000000201060 0000000000000000     0             register_tm_clones
    # 00000000002010a0 0000000000000000     0             __do_global_dtors_aux
    # 00000000002010c0 0000000000000000     0             frame_dummy
    # 00000000002010ed 0000000000000071     1         a.o:(.text)
    # 00000000002010ed 0000000000000071     0             main
    syms = []
    cur_section = None
    cur_section_is_useful = False
    promoted_name_count = 0
    # |is_partial| indicates that an eligible Level 3 line should be used to
    # update |syms[-1].full_name| instead of creating a new symbol.
    is_partial = False
    # Assembly code can create consecutive Level 3 lines with |size == 0|. These
    # lines can represent
    #  (1) assembly functions (should form symbol), or
    #  (2) assembly labels (should NOT form symbol).
    # It seems (2) correlates with the presence of a leading Level 3 line with
    # |size > 0|. This gives rise to the following strategy: Each symbol S from
    # a Level 3 line suppresses Level 3 lines with |address| less than
    # |next_usable_address := S.address + S.size|.
    next_usable_address = 0

    # For Thin-LTO, a map from each address to the Thin-LTO cache file. This
    # provides hints downstream to identify object_paths for .L.ref.tmp symbols,
    # but is not useful in the final output. Therefore it's stored separately,
    # instead of being in Symbol.
    thin_map = {}

    tokenizer = self.Tokenize(lines)

    in_partitions = False
    in_jump_table = False
    jump_tables_count = 0
    jump_entries_count = 0

    for (line, address, size, level, span, tok) in tokenizer:
      # Level 1 data match the "Out" column. They specify sections or
      # PROVIDE_HIDDEN lines.
      if level == 1:
        # Ignore sections that belong to feature library partitions. Seeing a
        # partition name is an indicator that we've entered a list of feature
        # partitions. After these, a single .part.end section will follow to
        # reserve memory at runtime. Seeing the .part.end section also marks the
        # end of partition sections in the map file.
        if tok.endswith('_partition'):
          in_partitions = True
        elif tok == '.part.end':
          # Note that we want to retain .part.end section, so it's fine to
          # restart processing on this section, rather than the next one.
          in_partitions = False

        if in_partitions:
          # For now, completely ignore feature partitions.
          cur_section = None
          cur_section_is_useful = False
        else:
          if not tok.startswith('PROVIDE_HIDDEN'):
            self._section_ranges[tok] = (address, size)
          cur_section = tok
          # E.g., Want to convert "(.text._name)" -> "_name" later.
          mangled_start_idx = len(cur_section) + 2
          cur_section_is_useful = (
              cur_section in models.BSS_SECTIONS
              or cur_section in (models.SECTION_RODATA, models.SECTION_TEXT)
              or cur_section.startswith(models.SECTION_DATA))

      elif cur_section_is_useful:
        # Level 2 data match the "In" column. They specify object paths and
        # section names within objects, or '<internal>:...'.
        if level == 2:
          # E.g., 'path.o:(.text._name)' => ['path.o', '(.text._name)'].
          cur_obj, paren_value = tok.split(':')

          in_jump_table = '.L.cfi.jumptable' in paren_value
          if in_jump_table:
            # Store each CFI jump table as a Level 2 symbol, whose Level 3
            # details are discarded.
            jump_tables_count += 1
            cur_obj = ''  # Replaces 'lto.tmp' to prevent problem later.
            mangled_name = '** CFI jump table'
          else:
            # E.g., '(.text.unlikely._name)' -> '_name'.
            mangled_name = paren_value[mangled_start_idx:-1]
            cur_flags = _FlagsFromMangledName(mangled_name)
            is_partial = True
            # As of 2017/11 LLD does not distinguish merged strings from other
            # merged data. Feature request is filed under:
            # https://bugs.llvm.org/show_bug.cgi?id=35248
            if cur_obj == '<internal>':
              if cur_section == '.rodata':
                # Treat all <internal> sections within .rodata as as string
                # literals. Some may hold numeric constants or other data, but
                # there is currently no way to distinguish them.
                mangled_name = '** lld merge strings'
              else:
                # e.g. <internal>:(.text.thunk)
                mangled_name = '** ' + mangled_name

              is_partial = False
              cur_obj = None
            elif (cur_obj == 'lto.tmp' or 'thinlto-cache' in cur_obj
                  or '.lto.' in cur_obj):
              thin_map[address] = os.path.basename(cur_obj)
              cur_obj = None

          # Create a symbol here since there may be no ensuing Level 3 lines.
          # But if there are, then the symbol can be modified later as sym[-1].
          sym = models.Symbol(cur_section, size, address=address,
                              full_name=mangled_name, object_path=cur_obj,
                              flags=cur_flags)
          syms.append(sym)

          # Level 3 |address| is nested under Level 2, don't add |size|.
          next_usable_address = address

        # Level 3 data match the "Symbol" column. They specify symbol names or
        # special names such as '.L_MergeGlobals'. Annotations such as '$d',
        # '$t.42' also appear at Level 3, but they are consumed by |tokenizer|,
        # so don't appear hear.
        elif level == 3:
          # Handle .L.cfi.jumptable.
          if in_jump_table:
            # Level 3 entries in CFI jump tables are thunks with mangled names.
            # Extracting them as symbols is not worthwhile; we only store the
            # Level 2 symbol, and print the count for verbose output. For
            # counting, '__typeid_' entries are excluded since they're likely
            # just annotations.
            if not tok.startswith('__typeid_'):
              jump_entries_count += 1
            continue

          # Ignore anything with '.L_MergedGlobals' prefix. This seems to only
          # happen for ARM (32-bit) builds.
          if tok.startswith('.L_MergedGlobals'):
            continue

          # Use |span| to decide whether to use a Level 3 line for Symbols. This
          # is useful for two purposes:
          # * This is a better indicator than |size|, which can be 0 for
          #   assembly functions.
          # * If multiple Level 3 lines have the same starting address, this
          #   cause all but the last line to have |span > 0|. This dedups lines
          #   with identical symbol names (why do they exist?). Note that this
          #   also skips legitimate aliases, but that's desired because nm.py
          #   (downstream) assumes no aliases already exist.
          if span > 0:
            stripped_tok = demangle.StripLlvmPromotedGlobalNames(tok)
            if len(tok) != len(stripped_tok):
              promoted_name_count += 1
              tok = stripped_tok
            tok = _NormalizeName(tok)

            # Handle special case where a partial symbol consumes bytes before
            # the first Level 3 symbol.
            if is_partial and syms[-1].address < address:
              # Truncate the partial symbol and leave it without |full_name|.
              # The data from the current line will form a new symbol.
              syms[-1].size = address - syms[-1].address
              next_usable_address = address
              is_partial = False

            if is_partial:
              syms[-1].full_name = tok
              syms[-1].size = size if size > 0 else min(syms[-1].size, span)
              next_usable_address = address + syms[-1].size
              is_partial = False
            elif address >= next_usable_address:
              if tok.startswith('__typeid_'):
                assert size == 1
                if tok.endswith('_byte_array'):
                  # CFI byte array table: |size| is inaccurate, so use |span|.
                  size_to_use = span
                else:
                  # Likely '_global_addr' or '_unique_member'. These should be:
                  # * Skipped since they're in CFI tables.
                  # * Suppressed (via |next_usable_address|) by another Level 3
                  #   symbol.
                  # Anything that makes it here would be an anomaly worthy of
                  # investigation, so print warnings.
                  logging.warning('Unrecognized __typeid_ symbol at %08X',
                                  address)
                  continue
              else:
                # Prefer |size|, and only fall back to |span| if |size == 0|.
                size_to_use = size if size > 0 else span
              sym = models.Symbol(cur_section, size_to_use, address=address,
                                  full_name=tok, flags=cur_flags)
              syms.append(sym)

              # Suppress symbols with overlapping |address|. This eliminates
              # labels from assembly sources.
              next_usable_address = address + size_to_use
              if cur_obj is not None:
                syms[-1].object_path = cur_obj

        else:
          logging.error('Problem line: %r', line)

    if promoted_name_count:
      logging.info('Found %d promoted global names', promoted_name_count)
    if jump_tables_count:
      logging.info('Found %d CFI jump tables with %d total entries',
                   jump_tables_count, jump_entries_count)
    return self._section_ranges, syms, {'thin_map': thin_map}


def _DetectLto(lines):
  """Scans LLD linker map file and returns whether LTO was used."""
  # It's assumed that the first line in |lines| was consumed to determine that
  # LLD was used. Seek 'thinlto-cache' prefix or the string '.lto' within an
  # "indicator section" as indicator for LTO.
  found_indicator_section = False
  # Potential names of "main section". Only one gets used.
  indicator_section_set = set(['.rodata', '.ARM.exidx'])
  start_pos = -1
  for line in lines:
    # Shortcut to avoid regex: The first line seen (second line in file) should
    # start a section, and start with '.', e.g.:
    #     194      194       13     1 .interp
    # Assign |start_pos| as position of '.', and trim everything before!
    if start_pos < 0:
      start_pos = line.index('.')
    if len(line) < start_pos:
      continue
    line = line[start_pos:]
    tok = line.lstrip()  # Allow whitespace at right.
    indent_size = len(line) - len(tok)
    if indent_size == 0:  # Section change.
      if found_indicator_section:  # Exit if just visited "main section".
        break
      if tok.strip() in indicator_section_set:
        found_indicator_section = True
    elif indent_size == 8:
      if found_indicator_section:
        if tok.startswith('thinlto-cache') or '.lto.' in tok:
          return True
  return False


def _DetectLinkerName(lines):
  """Heuristic linker detection from partial scan of the linker map.

  Args:
    lines: Iterable of lines from the linker map.

  Returns:
    A coded linker name.
  """
  first_line = next(lines)

  if first_line.startswith('Address'):
    return 'lld-lto_v0' if _DetectLto(lines) else 'lld_v0'

  if first_line.lstrip().startswith('VMA'):
    return 'lld-lto_v1' if _DetectLto(lines) else 'lld_v1'

  if first_line.startswith('Archive member'):
    return 'gold'

  raise Exception('Invalid map file: ' + first_line)


def ParseLines(lines):
  """Parses a linker map file given an iterable of its lines.

  Returns:
    A tuple of (section_ranges, symbols, extras).
  """
  # Buffer 1000 lines for format detection.
  header = list(itertools.islice(lines, 1000))
  lines = itertools.chain(header, lines)
  linker_name = _DetectLinkerName(iter(header))
  logging.info('Detected map file of type %s', linker_name)
  if linker_name.startswith('lld'):
    inner_parser = MapFileParserLld(linker_name)
  elif linker_name == 'gold':
    inner_parser = MapFileParserGold()
  else:
    raise Exception('.map file is from a unsupported linker.')

  next(lines)  # Consume the first line of headers.
  section_ranges, syms, extras = inner_parser.Parse(lines)
  for sym in syms:
    if sym.object_path and not sym.object_path.endswith(')'):
      # Don't want '' to become '.'.
      # Thin archives' paths will get fixed in |ar.CreateThinObjectPath|.
      sym.object_path = os.path.normpath(sym.object_path)
  return section_ranges, syms, extras


def ParseFile(path):
  """Parses a linker map file pointed to by |path|.

  Returns:
    A tuple of (section_ranges, symbols, extras).
  """
  with _OpenMaybeGzAsText(path) as f:
    return ParseLines(f)


def DeduceObjectPathsFromThinMap(raw_symbols, extras):
  """Uses Thin-LTO object paths to find object_paths of symbols. """
  thin_map = extras.get('thin_map', None)  # |address| -> |thin_obj|
  if not thin_map:  # None or empty.
    logging.info('No thin-object-path found: Skipping object path deduction.')
    return

  # Build map of |thin_obj| -> |object_paths|.
  thin_obj_to_object_paths = collections.defaultdict(set)
  logging.info('Building map of thin-object-path -> object path.')
  for symbol in raw_symbols:
    if symbol.object_path:
      thin_obj = thin_map.get(symbol.address, None)
      if thin_obj:
        thin_obj_to_object_paths[thin_obj].add(symbol.object_path)

  # For each symbol without |object_path|, translate |address| -> |thin_obj| ->
  # |object_paths|. If unique, then assign to symbol. Stats are kept, keyed on
  # |len(object_paths)|.
  # Example symbols this happens with: ".L.ref.tmp", "** outlined function".
  logging.info('Assigning object paths to using ThinLTO paths.')
  ref_tmp_popu = [0] * 3
  ref_tmp_pss = [0] * 3
  for symbol in raw_symbols:
    if not symbol.object_path:
      thin_obj = thin_map.get(symbol.address)
      # Ignore non-native symbols.
      if thin_obj:
        count = 0
        object_paths = thin_obj_to_object_paths.get(thin_obj)
        if object_paths is not None:
          count = min(len(object_paths), 2)  # 2+ maps to 2.
          # We could create path aliases when count > 1, but it wouldn't
          # necessarily be correct. That occurs when *another* symbol from the
          # same .o file contains a path alias, but not necessarily this symbol.
          if count == 1:
            symbol.object_path = next(iter(object_paths))
        ref_tmp_popu[count] += 1
        ref_tmp_pss[count] += symbol.pss

  # As of Mar 2019:
  #   No match: 2 symbols with total PSS = 20
  #   Assigned (1 object path): 1098 symbols with total PSS = 55454
  #   Ambiguous (2+ object paths): 2315 symbols with total PSS = 41941
  logging.info('Object path deduction results for pathless symbols:')
  logging.info('  No match: %d symbols with total PSS = %d', ref_tmp_popu[0],
               ref_tmp_pss[0])
  logging.info('  Assigned (1 object path): %d symbols with total PSS = %d',
               ref_tmp_popu[1], ref_tmp_pss[1])
  logging.info('  Ambiguous (2+ object paths): %d symbols with total PSS = %d',
               ref_tmp_popu[2], ref_tmp_pss[2])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('linker_file', type=os.path.realpath)
  parser.add_argument(
      '-v',
      '--verbose',
      default=0,
      action='count',
      help='Verbose level (multiple times for more)')
  parser.add_argument('--dump', action='store_true')
  args = parser.parse_args()

  logging.basicConfig(
      level=logging.WARNING - args.verbose * 10,
      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  section_ranges, syms, extras = ParseFile(args.linker_file)

  if args.dump:
    print(section_ranges)
    for sym in syms:
      print(sym)
  else:
    # Enter interactive shell.
    readline.parse_and_bind('tab: complete')
    variables = {
        'section_ranges': section_ranges,
        'syms': syms,
        'extras': extras
    }
    banner_lines = [
        '*' * 80,
        'Variables:',
        '  section_ranges: Map from section name to (address, size).',
        '  syms: Raw symbols parsed from the linker map file.',
        '  extras: Format-specific extra data.',
        '*' * 80,
    ]
    code.InteractiveConsole(variables).interact('\n'.join(banner_lines))


if __name__ == '__main__':
  main()
