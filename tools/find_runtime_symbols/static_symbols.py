# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bisect
import re


_ARGUMENT_TYPE_PATTERN = re.compile('\([^()]*\)(\s*const)?')
_TEMPLATE_ARGUMENT_PATTERN = re.compile('<[^<>]*>')
_LEADING_TYPE_PATTERN = re.compile('^.*\s+(\w+::)')
_READELF_SECTION_HEADER_PATTER = re.compile(
    '^\s*\[\s*(Nr|\d+)\]\s+(|\S+)\s+([A-Z_]+)\s+([0-9a-f]+)\s+'
    '([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9]+)\s+([WAXMSILGxOop]*)\s+'
    '([0-9]+)\s+([0-9]+)\s+([0-9]+)')


class ParsingException(Exception):
  def __str__(self):
    return repr(self.args[0])


class AddressMapping:
  def __init__(self):
    self._symbol_map = {}

  def append(self, start, entry):
    self._symbol_map[start] = entry

  def find(self, address):
    return self._symbol_map.get(address)


class RangeAddressMapping(AddressMapping):
  def __init__(self):
    super().__init__()
    self._sorted_start_list = []
    self._is_sorted = True

  def append(self, start, entry):
    if self._sorted_start_list:
      if self._sorted_start_list[-1] > start:
        self._is_sorted = False
      elif self._sorted_start_list[-1] == start:
        return
    self._sorted_start_list.append(start)
    self._symbol_map[start] = entry

  def find(self, address):
    if not self._sorted_start_list:
      return None
    if not self._is_sorted:
      self._sorted_start_list.sort()
      self._is_sorted = True
    found_index = bisect.bisect_left(self._sorted_start_list, address)
    found_start_address = self._sorted_start_list[found_index - 1]
    return self._symbol_map[found_start_address]


class Procedure:
  """A class for a procedure symbol and an address range for the symbol."""

  def __init__(self, start, end, name):
    self.start = start
    self.end = end
    self.name = name

  def __eq__(self, other):
    return (self.start == other.start and
            self.end == other.end and
            self.name == other.name)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __str__(self):
    return '%x-%x: %s' % (self.start, self.end, self.name)


class ElfSection:
  """A class for an elf section header."""

  def __init__(
      self, number, name, stype, address, offset, size, es, flg, lk, inf, al):
    self.number = number
    self.name = name
    self.stype = stype
    self.address = address
    self.offset = offset
    self.size = size
    self.es = es
    self.flg = flg
    self.lk = lk
    self.inf = inf
    self.al = al

  def __eq__(self, other):
    return (self.number == other.number and
            self.name == other.name and
            self.stype == other.stype and
            self.address == other.address and
            self.offset == other.offset and
            self.size == other.size and
            self.es == other.es and
            self.flg == other.flg and
            self.lk == other.lk and
            self.inf == other.inf and
            self.al == other.al)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __str__(self):
    return '%x+%x(%x) %s' % (self.address, self.size, self.offset, self.name)


class StaticSymbolsInFile:
  """Represents static symbol information in a binary file."""

  def __init__(self, my_name):
    self.my_name = my_name
    self._elf_sections = []
    self._procedures = RangeAddressMapping()
    self._sourcefiles = RangeAddressMapping()
    self._typeinfos = AddressMapping()

  def _append_elf_section(self, elf_section):
    self._elf_sections.append(elf_section)

  def _append_procedure(self, start, procedure):
    self._procedures.append(start, procedure)

  def _append_sourcefile(self, start, sourcefile):
    self._sourcefiles.append(start, sourcefile)

  def _append_typeinfo(self, start, typeinfo):
    self._typeinfos.append(start, typeinfo)

  def _find_symbol_by_runtime_address(self, address, vma, target):
    if not (vma.begin <= address < vma.end):
      return None

    if vma.name != self.my_name:
      return None

    file_offset = address - (vma.begin - vma.offset)
    elf_address = None
    for section in self._elf_sections:
      if section.offset <= file_offset < (section.offset + section.size):
        elf_address = section.address + file_offset - section.offset
    if not elf_address:
      return None

    return target.find(elf_address)

  def find_procedure_by_runtime_address(self, address, vma):
    return self._find_symbol_by_runtime_address(address, vma, self._procedures)

  def find_sourcefile_by_runtime_address(self, address, vma):
    return self._find_symbol_by_runtime_address(address, vma, self._sourcefiles)

  def find_typeinfo_by_runtime_address(self, address, vma):
    return self._find_symbol_by_runtime_address(address, vma, self._typeinfos)

  def load_readelf_ew(self, f):
    found_header = False
    for line in f:
      if line.rstrip() == 'Section Headers:':
        found_header = True
        break
    if not found_header:
      return

    for line in f:
      line = line.rstrip()
      matched = _READELF_SECTION_HEADER_PATTER.match(line)
      if matched:
        self._append_elf_section(ElfSection(
            int(matched.group(1), 10), # number
            matched.group(2), # name
            matched.group(3), # stype
            int(matched.group(4), 16), # address
            int(matched.group(5), 16), # offset
            int(matched.group(6), 16), # size
            matched.group(7), # es
            matched.group(8), # flg
            matched.group(9), # lk
            matched.group(10), # inf
            matched.group(11) # al
            ))
      else:
        if line in ('Key to Flags:', 'Program Headers:'):
          break

  def load_readelf_debug_decodedline_file(self, input_file):
    for line in input_file:
      splitted = line.rstrip().split(None, 2)
      self._append_sourcefile(int(splitted[0], 16), splitted[1])

  @staticmethod
  def _parse_nm_bsd_line(line):
    if line[8] == ' ':
      return line[0:8], line[9], line[11:]
    if line[16] == ' ':
      return line[0:16], line[17], line[19:]
    raise ParsingException('Invalid nm output.')

  @staticmethod
  def _get_short_function_name(function):
    while True:
      function, number = _ARGUMENT_TYPE_PATTERN.subn('', function)
      if not number:
        break
    while True:
      function, number = _TEMPLATE_ARGUMENT_PATTERN.subn('', function)
      if not number:
        break
    return _LEADING_TYPE_PATTERN.sub('\g<1>', function)

  def load_nm_bsd(self, f, mangled=False):
    last_start = 0
    routine = ''

    for line in f:
      line = line.rstrip()
      sym_value, sym_type, sym_name = self._parse_nm_bsd_line(line)

      if sym_value[0] == ' ':
        continue

      start_val = int(sym_value, 16)

      if (sym_type in ('r', 'R', 'D', 'U', 'd', 'V') and
          (not mangled and sym_name.startswith('typeinfo'))):
        self._append_typeinfo(start_val, sym_name)

      # It's possible for two symbols to share the same address, if
      # one is a zero-length variable (like __start_google_malloc) or
      # one symbol is a weak alias to another (like __libc_malloc).
      # In such cases, we want to ignore all values except for the
      # actual symbol, which in nm-speak has type "T".  The logic
      # below does this, though it's a bit tricky: what happens when
      # we have a series of lines with the same address, is the first
      # one gets queued up to be processed.  However, it won't
      # *actually* be processed until later, when we read a line with
      # a different address.  That means that as long as we're reading
      # lines with the same address, we have a chance to replace that
      # item in the queue, which we do whenever we see a 'T' entry --
      # that is, a line with type 'T'.  If we never see a 'T' entry,
      # we'll just go ahead and process the first entry (which never
      # got touched in the queue), and ignore the others.
      if start_val == last_start and sym_type in ('t', 'T'):
        # We are the 'T' symbol at this address, replace previous symbol.
        routine = sym_name
        continue
      if start_val == last_start:
        # We're not the 'T' symbol at this address, so ignore us.
        continue

      # Tag this routine with the starting address in case the image
      # has multiple occurrences of this routine.  We use a syntax
      # that resembles template paramters that are automatically
      # stripped out by ShortFunctionName()
      sym_name += "<%016x>" % start_val

      if not mangled:
        routine = self._get_short_function_name(routine)
      self._append_procedure(
          last_start, Procedure(last_start, start_val, routine))

      last_start = start_val
      routine = sym_name

    if not mangled:
      routine = self._get_short_function_name(routine)
    self._append_procedure(
        last_start, Procedure(last_start, last_start, routine))
