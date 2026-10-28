# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to extract string literals from object files.

LookupElfRodataInfo():
  Runs readelf to extract and return .rodata section spec of an ELF file.

ReadFileChunks():
  Reads raw data from a file, given a list of ranges in the file.

ReadStringLiterals():
  Reads the ELF file to find the string contents of a list of string literals.

ResolveStringPiecesIndirect():
  BulkForkAndCall() target: Given {path: [string addresses]} and
  StringDataIndex of [raw_string_data for each string_section]:
  - Reads {path: [src_strings]}.
  - For each path, searches for src_strings in at most 1 raw_string_data over
    each string_section. If found, translates to string_range and annotates it
    to the string_section.
  - Returns [{path: [string_ranges]} for each string_section].

ResolveStringPieces():
  BulkForkAndCall() target: Given {path: [strings]} and
  StringDataIndex of [raw_string_data for each string_section]:
  - For each path, searches for src_strings in at most 1 raw_string_data over
    each string_section. If found, translates to string_range and annotates it
    to the string_section.
  - Returns [{path: [string_ranges]} for each string_section].

GetNameOfStringLiteralBytes():
  Converts string literal bytes to printable form, useful for assigning
  full_name of string literal symbols. If any non-printable character is found
  then returns models.STRING_LITERAL_NAME. Otherwise the returned string is
  quoted, and may be truncated (with "[...]" appended).
"""

import ast
import bisect
import collections
import itertools
import logging
import os
import string
import subprocess

import ar
import models
import parallel
import path_util


_STRING_LITERAL_LENGTH_CUTOFF = 30


def LookupElfRodataInfo(elf_path):
  """Returns (address, offset, size) for the .rodata section."""
  args = [path_util.GetReadElfPath(), '-S', '--wide', elf_path]
  output = subprocess.check_output(args).decode('ascii')
  lines = output.splitlines()
  for line in lines:
    # [Nr] Name           Type        Addr     Off     Size   ES Flg Lk Inf Al
    # [07] .rodata        PROGBITS    025e7000 237c000 5ec4f6 00   A  0   0 256
    if '.rodata ' in line:
      fields = line[line.index(models.SECTION_RODATA) :].split()
      return int(fields[2], 16), int(fields[3], 16), int(fields[4], 16)
  raise AssertionError('No .rodata for command: ' + repr(args))


def ReadFileChunks(path, section_ranges):
  """Returns a list of raw data from |path|, specified by |section_ranges|.

  Args:
    section_ranges: List of (offset, size).
  """
  ret = []
  if not section_ranges:
    return ret
  with open(path, 'rb') as f:
    for offset, size in section_ranges:
      f.seek(offset)
      ret.append(f.read(size))
  return ret


def _ExtractArchivePath(path):
  # E.g. foo/bar.a(baz.o)
  if path.endswith(')'):
    start_idx = path.index('(')
    return path[:start_idx]
  return None


def _LookupStringSectionPositions(target, output_directory):
  """Returns a dict of object_path -> [(offset, size)...] of .rodata sections.

  Args:
    target: An archive path string (e.g., "foo.a") or a list of object paths.
  """
  is_archive = isinstance(target, str)
  args = [path_util.GetReadElfPath(), '-S', '--wide']
  if is_archive:
    args.append(target)
  else:
    # Assign path for when len(target) == 1, (no File: line exists).
    path = target[0]
    args.extend(target)

  output = subprocess.check_output(args, cwd=output_directory).decode('ascii')
  lines = output.splitlines()
  section_positions_by_path = {}
  cur_offsets = []
  for line in lines:
    # File: base/third_party/libevent/libevent.a(buffer.o)
    # [Nr] Name              Type        Addr     Off    Size   ES Flg Lk Inf Al
    # [11] .rodata.str1.1    PROGBITS    00000000 0000b4 000004 01 AMS  0   0  1
    # [11] .rodata.str4.4    PROGBITS    00000000 0000b4 000004 01 AMS  0   0  4
    # [11] .rodata.str8.8    PROGBITS    00000000 0000b4 000004 01 AMS  0   0  8
    # [80] .rodata..L.str    PROGBITS    00000000 000530 000002 00   A  0   0  1
    # The various string sections differ by alignment.
    # The presence of a wchar_t literal (L"asdf") seems to make a str4 section.
    # When multiple sections exist, nm gives us no indication as to which
    # section each string corresponds to.
    if line.startswith('File: '):
      if cur_offsets:
        section_positions_by_path[path] = cur_offsets
        cur_offsets = []
      path = line[6:]
    elif '.rodata.' in line:
      progbits_idx = line.find('PROGBITS ')
      if progbits_idx != -1:
        fields = line[progbits_idx:].split()
        position = (int(fields[2], 16), int(fields[3], 16))
        # The heuristics in _IterStringLiterals rely on str1 coming first.
        if fields[-1] == '1':
          cur_offsets.insert(0, position)
        else:
          cur_offsets.append(position)
  if cur_offsets:
    section_positions_by_path[path] = cur_offsets
  return section_positions_by_path


def _ReadStringSections(target, output_directory, positions_by_path):
  """Returns a dict of object_path -> [string...] of .rodata chunks.

  Args:
    target: An archive path string (e.g., "foo.a") or a list of object paths.
    positions_by_path: A dict of object_path -> [(offset, size)...]
  """
  is_archive = isinstance(target, str)
  string_sections_by_path = {}
  if is_archive:
    for subpath, chunk in ar.IterArchiveChunks(
      os.path.join(output_directory, target)
    ):
      path = '{}({})'.format(target, subpath)
      positions = positions_by_path.get(path)
      # No positions if file has no string literals.
      if positions:
        string_sections_by_path[path] = [
          chunk[offset : offset + size] for offset, size in positions
        ]
  else:
    for path in target:
      positions = positions_by_path.get(path)
      # We already log a warning about this in _IterStringLiterals().
      if positions:
        string_sections_by_path[path] = ReadFileChunks(
          os.path.join(output_directory, path), positions
        )
  return string_sections_by_path


def _IterStringLiterals(path, addresses, obj_sections):
  """Yields all string literals (including \0) for the given object path.

  Args:
    path: Object file path.
    addresses: List of string offsets encoded as hex strings.
    obj_sections: List of contents of .rodata.str sections read from the given
        object file.
  """

  next_offsets = sorted(int(a, 16) for a in addresses)
  if not obj_sections:
    # Happens when there is an address for a symbol which is not actually a
    # string literal, or when string_sections_by_path is missing an entry.
    logging.warning(
      'Object has %d strings but no string sections: %s', len(addresses), path
    )
    return
  for section_data in obj_sections:
    cur_offsets = next_offsets
    # Always assume first element is 0. I'm not entirely sure why this is
    # necessary, but strings get missed without it.
    next_offsets = [0]
    prev_offset = 0
    # TODO(agrieve): Switch to using nm --print-size in order to capture the
    #     address+size of each string rather than just the address.
    for offset in cur_offsets[1:]:
      if offset >= len(section_data):
        # Remaining offsets are for next section.
        next_offsets.append(offset)
        continue
      # Figure out which offsets apply to this section via heuristic of them
      # all ending with a null character.
      if offset == prev_offset or section_data[offset - 1] != 0:
        next_offsets.append(offset)
        continue
      yield section_data[prev_offset:offset]
      prev_offset = offset

    if prev_offset < len(section_data):
      yield section_data[prev_offset:]


def _Bigrams(data):
  """Returns the set of all 2-byte sequences in |data| (as ints)."""
  n = len(data)
  mv = memoryview(data)
  even = mv[: n // 2 * 2].cast('H')
  odd = mv[1 : 1 + (n - 1) // 2 * 2].cast('H')
  return set(even).union(odd)


class StringDataIndex:
  """Indexes ** merge strings sections for fast lookup of string literals.

  Sections consist of \\0-terminated pieces. A \\0-terminated value with no
  other \\0s can match only an entire piece ("exact" match), or a proper suffix
  of a piece.
  """

  def __init__(self, string_data):
    self.string_data = string_data
    # [section_idx] -> {piece: offset of first occurrence}.
    self._offset_by_piece = []
    # [section_idx] -> Sorted list of reversed (unique) pieces.
    self._sorted_reversed_pieces = []
    # [section_idx] -> End offsets of pieces in |_sorted_reversed_pieces|.
    self._sorted_end_offsets = []
    for data in string_data:
      offset_by_piece = {}
      offset = 0
      # The last item is not \0-terminated, and so can never match.
      for piece in data.split(b'\0')[:-1]:
        offset_by_piece.setdefault(piece, offset)
        offset += len(piece) + 1
      tups = sorted((p[::-1], o + len(p)) for p, o in offset_by_piece.items())
      self._offset_by_piece.append(offset_by_piece)
      self._sorted_reversed_pieces.append([t[0] for t in tups])
      self._sorted_end_offsets.append([t[1] for t in tups])
    # [section_idx] -> Set of 2-byte sequences (as ints) within the section.
    # Used to quickly rule out values that do not appear in a section.
    self._bigrams = [_Bigrams(data) for data in string_data]

  def _FindSuffixMatch(self, section_idx, piece):
    """Returns the offset of the first |piece|\\0, or -1 if there is none.

    Assumes that |piece| does not exist as an entire piece.
    """
    sorted_reversed_pieces = self._sorted_reversed_pieces[section_idx]
    reversed_piece = piece[::-1]
    start_idx = bisect.bisect_left(sorted_reversed_pieces, reversed_piece)
    # Find the end of the range of pieces that start with |reversed_piece| by
    # searching for the smallest value that is greater than all of them.
    upper_bound = reversed_piece.rstrip(b'\xff')
    if upper_bound:
      upper_bound = upper_bound[:-1] + bytes((upper_bound[-1] + 1,))
      end_idx = bisect.bisect_left(
        sorted_reversed_pieces, upper_bound, lo=start_idx
      )
    else:
      end_idx = len(sorted_reversed_pieces)
    if end_idx == start_idx:
      return -1
    end_offsets = self._sorted_end_offsets[section_idx]
    return min(end_offsets[start_idx:end_idx]) - len(piece)

  def Find(self, value):
    """Returns (section_idx, offset) of |value|, or (-1, -1) if not found.

    Prefers matches that follow a \\0 (or are at offset 0). Otherwise, returns
    the first match within the first section that contains |value|.
    """
    if not value or value.find(b'\0') != len(value) - 1:
      return self._FindSlow(value)
    piece = value[:-1]
    for section_idx, offset_by_piece in enumerate(self._offset_by_piece):
      offset = offset_by_piece.get(piece, -1)
      if offset != -1:
        return section_idx, offset
    for section_idx in range(len(self.string_data)):
      offset = self._FindSuffixMatch(section_idx, piece)
      if offset != -1:
        return section_idx, offset
    return -1, -1

  def _FindSlow(self, value):
    first_match = (-1, -1)
    value_bigrams = _Bigrams(value)
    for section_idx, data in enumerate(self.string_data):
      if not value_bigrams <= self._bigrams[section_idx]:
        continue
      # Set offset so that it will be 0 when len(value) is added to it below.
      offset = -len(value)
      while True:
        offset = data.find(value, offset + len(value))
        if offset == -1:
          break
        # Preferring exact matches (those following \0) over substring matches
        # significantly increases accuracy (although shows that linker isn't
        # being optimal).
        if offset == 0 or data[offset - 1] == 0:
          return section_idx, offset
        if first_match[0] == -1:
          first_match = (section_idx, offset)
    # Exact match not found, so take suffix match if it exists.
    return first_match


def _AnnotateStringData(string_data_index, path_value_gen):
  """Annotates each |string_data| section data with paths and ranges.

  Args:
    string_data_index: StringDataIndex of [raw_string_data for each
      string_section] from an ELF file.
    path_value_gen: A generator of (path, value) pairs, where |path|
      is the path to an object file and |value| is a string to annotate.

  Returns:
    [{path: [string_ranges]} for each string_section].
  """
  ret = [collections.defaultdict(list) for _ in string_data_index.string_data]
  find = string_data_index.Find
  for path, value in path_value_gen:
    section_idx, offset = find(value)
    # Missing strings happen when optimization make them unused.
    if offset != -1:
      # Encode tuple as a string for easier mashalling.
      ret[section_idx][path].append(str(offset) + ':' + str(len(value)))

  return ret


# This is a target for BulkForkAndCall().
def ResolveStringPiecesIndirect(
  encoded_string_addresses_by_path, string_data_index, output_directory
):
  string_addresses_by_path = parallel.DecodeDictOfLists(
    encoded_string_addresses_by_path
  )
  # Assign |target| as archive path, or a list of object paths.
  any_path = next(iter(string_addresses_by_path.keys()))
  target = _ExtractArchivePath(any_path)
  if not target:
    target = list(string_addresses_by_path.keys())

  # Run readelf to find location of .rodata within the .o files.
  section_positions_by_path = _LookupStringSectionPositions(
    target, output_directory
  )
  # Load the .rodata sections (from object files) as strings.
  string_sections_by_path = _ReadStringSections(
    target, output_directory, section_positions_by_path
  )

  def GeneratePathAndValues():
    for path, object_addresses in string_addresses_by_path.items():
      for value in _IterStringLiterals(
        path, object_addresses, string_sections_by_path.get(path)
      ):
        yield path, value

  ret = _AnnotateStringData(string_data_index, GeneratePathAndValues())
  return [parallel.EncodeDictOfLists(x) for x in ret]


# This is a target for BulkForkAndCall().
def ResolveStringPieces(encoded_strings_by_path, string_data_index):
  # ast.literal_eval() undoes repr() applied to strings.
  strings_by_path = parallel.DecodeDictOfLists(
    encoded_strings_by_path, value_transform=ast.literal_eval
  )

  def GeneratePathAndValues():
    for path, strings in strings_by_path.items():
      for value in strings:
        yield path, value

  ret = _AnnotateStringData(string_data_index, GeneratePathAndValues())
  return [parallel.EncodeDictOfLists(x) for x in ret]


def ReadStringLiterals(symbols, elf_path, all_rodata=False):
  """Returns an iterable of (symbol, data) for all string literal symbols.

  Emitted string literal data are null-terminated bytes.

  Args:
    symbols: An iterable of Symbols
    elf_path: Path to the executable containing the symbols.
    all_rodata: Assume every symbol within .rodata that ends with a \0 is a
         string literal.
  """
  address, offset, _ = LookupElfRodataInfo(elf_path)
  adjust = offset - address
  with open(elf_path, 'rb') as f:
    for symbol in symbols:
      if symbol.section != 'r':
        continue
      f.seek(symbol.address + adjust)
      data = f.read(symbol.size_without_padding)
      # As of Oct 2017, there are ~90 symbols name .L.str(.##). These appear
      # in the linker map file explicitly, and there doesn't seem to be a
      # pattern as to which variables lose their kConstant name (the more
      # common case), or which string literals don't get moved to
      # ** merge strings (less common).
      if symbol.IsStringLiteral() or (all_rodata and data and data[-1] == 0):
        yield ((symbol, data))


def GetNameOfStringLiteralBytes(b):
  """Converts string literal bytes to printable form, may be truncated."""
  # Do an extra initial truncation to make the ascii checks faster.
  b = b[: _STRING_LITERAL_LENGTH_CUTOFF + 1]
  if not b.isascii():
    # file_format.py currently requires ascii (maybe unnecessarily...)
    return models.STRING_LITERAL_NAME
  b = b.replace(b'\r', b'').replace(b'\n', b'').replace(b'\t', b'')
  b = b.rstrip(b'\00')
  s = b.decode('ascii')
  if s.isprintable():
    if len(s) > _STRING_LITERAL_LENGTH_CUTOFF:
      return f'"{s[: _STRING_LITERAL_LENGTH_CUTOFF - 3]}"...'
    return f'"{s}"'
  return models.STRING_LITERAL_NAME
