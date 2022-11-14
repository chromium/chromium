#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for optimistically parsing dex files.

This file is not meant to provide a generic tool for analyzing dex files.
A DexFile class that exposes access to several memory items in the dex format
is provided, but it does not include error handling or validation.
"""

import argparse
import collections
import errno
import os
import re
import struct
import sys
import zipfile

# https://source.android.com/devices/tech/dalvik/dex-format#header-item
_DEX_HEADER_FMT = (
    ('magic', '8s'),
    ('checksum', 'I'),
    ('signature', '20s'),
    ('file_size', 'I'),
    ('header_size', 'I'),
    ('endian_tag', 'I'),
    ('link_size', 'I'),
    ('link_off', 'I'),
    ('map_off', 'I'),
    ('string_ids_size', 'I'),
    ('string_ids_off', 'I'),
    ('type_ids_size', 'I'),
    ('type_ids_off', 'I'),
    ('proto_ids_size', 'I'),
    ('proto_ids_off', 'I'),
    ('field_ids_size', 'I'),
    ('field_ids_off', 'I'),
    ('method_ids_size', 'I'),
    ('method_ids_off', 'I'),
    ('class_defs_size', 'I'),
    ('class_defs_off', 'I'),
    ('data_size', 'I'),
    ('data_off', 'I'),
)

DexHeader = collections.namedtuple('DexHeader',
                                   ','.join(t[0] for t in _DEX_HEADER_FMT))

# Simple memory items.
_TypeIdItem = collections.namedtuple('TypeIdItem', 'descriptor_idx')
_ProtoIdItem = collections.namedtuple(
    'ProtoIdItem', 'shorty_idx,return_type_idx,parameters_off')
_MethodIdItem = collections.namedtuple('MethodIdItem',
                                       'type_idx,proto_idx,name_idx')
_TypeItem = collections.namedtuple('TypeItem', 'type_idx')
_StringDataItem = collections.namedtuple('StringItem', 'utf16_size,data')
_ClassDefItem = collections.namedtuple(
    'ClassDefItem',
    'class_idx,access_flags,superclass_idx,interfaces_off,source_file_idx,'
    'annotations_off,class_data_off,static_values_off')


class _MemoryItemList:
  """Base class for repeated memory items."""

  def __init__(self,
               reader,
               offset,
               size,
               factory,
               alignment=None,
               first_item_offset=None):
    """Creates the item list using the specific item factory.

    Args:
      reader: _DexReader used for decoding the memory item.
      offset: Offset from start of the file to the item list, serving as the
        key for some item types.
      size: Number of memory items in the list.
      factory: Function to extract each memory item from a _DexReader.
      alignment: Optional integer specifying the alignment for the memory
        section represented by this list.
      first_item_offset: Optional, specifies a different offset to use for
        extracting memory items (default is to use offset).
    """
    self.offset = offset
    self.size = size
    reader.Seek(first_item_offset or offset)
    self._items = [factory(reader) for _ in range(size)]

    if alignment:
      reader.AlignUpTo(alignment)

  def __iter__(self):
    return iter(self._items)

  def __getitem__(self, key):
    return self._items[key]

  def __len__(self):
    return len(self._items)

  def __repr__(self):
    item_type_part = ''
    if self.size != 0:
      item_type = type(self._items[0])
      item_type_part = ', item type={}'.format(item_type.__name__)

    return '{}(offset={:#x}, size={}{})'.format(
        type(self).__name__, self.offset, self.size, item_type_part)


class _TypeIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = lambda x: _TypeIdItem(x.ReadUInt())
    super().__init__(reader, offset, size, factory)


class _ProtoIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = lambda x: _ProtoIdItem(x.ReadUInt(), x.ReadUInt(), x.ReadUInt())
    super().__init__(reader, offset, size, factory)


class _MethodIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = (
        lambda x: _MethodIdItem(x.ReadUShort(), x.ReadUShort(), x.ReadUInt()))
    super().__init__(reader, offset, size, factory)


class _StringItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)
    string_item_offsets = iter([reader.ReadUInt() for _ in range(size)])

    def factory(x):
      data_offset = next(string_item_offsets)
      string = x.ReadString(data_offset)
      return _StringDataItem(len(string), string)

    super().__init__(reader, offset, size, factory)


class _TypeListItem(_MemoryItemList):
  def __init__(self, reader):
    offset = reader.Tell()
    size = reader.ReadUInt()
    factory = lambda x: _TypeItem(x.ReadUShort())
    # This is necessary because we need to extract the size of the type list
    # (in other cases the list size is provided in the header).
    first_item_offset = reader.Tell()
    super().__init__(reader,
                     offset,
                     size,
                     factory,
                     alignment=4,
                     first_item_offset=first_item_offset)


class _TypeListItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    super().__init__(reader, offset, size, _TypeListItem)


class _ClassDefItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)

    def factory(x):
      return _ClassDefItem(*(x.ReadUInt()
                             for _ in range(len(_ClassDefItem._fields))))

    super().__init__(reader, offset, size, factory)


class _DexMapItem:
  def __init__(self, reader):
    self.type = reader.ReadUShort()
    reader.ReadUShort()
    self.size = reader.ReadUInt()
    self.offset = reader.ReadUInt()

  def __repr__(self):
    return '_DexMapItem(type={}, size={}, offset={:#x})'.format(
        self.type, self.size, self.offset)


class _DexMapList:
  # Full list of type codes:
  # https://source.android.com/devices/tech/dalvik/dex-format#type-codes
  TYPE_TYPE_LIST = 0x1001

  def __init__(self, reader, offset):
    self._map = {}
    reader.Seek(offset)
    self._size = reader.ReadUInt()
    for _ in range(self._size):
      item = _DexMapItem(reader)
      self._map[item.type] = item

  def __getitem__(self, key):
    return self._map[key]

  def __contains__(self, key):
    return key in self._map

  def __repr__(self):
    return '_DexMapList(size={}, items={})'.format(self._size, self._map)


class _DexReader:
  def __init__(self, data):
    self._data = data
    self._pos = 0

  def Seek(self, offset):
    self._pos = offset

  def Tell(self):
    return self._pos

  def ReadUByte(self):
    return self._ReadData('<B')

  def ReadUShort(self):
    return self._ReadData('<H')

  def ReadUInt(self):
    return self._ReadData('<I')

  def ReadString(self, data_offset):
    string_length, string_offset = self._ReadULeb128(data_offset)
    string_data_offset = string_offset + data_offset
    return self._DecodeMUtf8(string_length, string_data_offset)

  def AlignUpTo(self, align_unit):
    off_by = self._pos % align_unit
    if off_by:
      self.Seek(self._pos + align_unit - off_by)

  def ReadHeader(self):
    header_fmt = '<' + ''.join(t[1] for t in _DEX_HEADER_FMT)
    return DexHeader._make(struct.unpack_from(header_fmt, self._data))

  def _ReadData(self, fmt):
    ret = struct.unpack_from(fmt, self._data, self._pos)[0]
    self._pos += struct.calcsize(fmt)
    return ret

  def _ReadULeb128(self, data_offset):
    """Returns a tuple of (uleb128 value, number of bytes occupied).

    From DWARF3 spec: http://dwarfstd.org/doc/Dwarf3.pdf

    Args:
      data_offset: Location of the unsigned LEB128.
    """
    value = 0
    shift = 0
    cur_offset = data_offset
    while True:
      byte = self._data[cur_offset]
      cur_offset += 1
      value |= (byte & 0b01111111) << shift
      if (byte & 0b10000000) == 0:
        break
      shift += 7

    return value, cur_offset - data_offset

  def _DecodeMUtf8(self, string_length, offset):
    """Returns the string located at the specified offset.

    See https://source.android.com/devices/tech/dalvik/dex-format#mutf-8

    Ported from the Android Java implementation:
    https://android.googlesource.com/platform/dalvik/+/fe107fb6e3f308ac5174ebdc5a794ee880c741d9/dx/src/com/android/dex/Mutf8.java#34

    Args:
      string_length: The length of the decoded string.
      offset: Offset to the beginning of the string.
    """
    self.Seek(offset)
    ret = ''

    for _ in range(string_length):
      a = self.ReadUByte()
      if a == 0:
        raise _MUTf8DecodeError('Early string termination encountered',
                                string_length, offset)
      if (a & 0x80) == 0x00:
        code = a
      elif (a & 0xe0) == 0xc0:
        b = self.ReadUByte()
        if (b & 0xc0) != 0x80:
          raise _MUTf8DecodeError('Error in byte 2', string_length, offset)
        code = ((a & 0x1f) << 6) | (b & 0x3f)
      elif (a & 0xf0) == 0xe0:
        b = self.ReadUByte()
        c = self.ReadUByte()
        if (b & 0xc0) != 0x80 or (c & 0xc0) != 0x80:
          raise _MUTf8DecodeError('Error in byte 3 or 4', string_length, offset)
        code = ((a & 0x0f) << 12) | ((b & 0x3f) << 6) | (c & 0x3f)
      else:
        raise _MUTf8DecodeError('Bad byte', string_length, offset)
      ret += chr(code)

    if self.ReadUByte() != 0x00:
      raise _MUTf8DecodeError('Expected string termination', string_length,
                              offset)

    return ret


class _MUTf8DecodeError(Exception):
  def __init__(self, message, length, offset):
    message += ' (decoded string length: {}, string data offset: {:#x})'.format(
        length, offset)
    super().__init__(message)


class DexFile:
  """Represents a single dex file.

  Parses and exposes access to dex file structure and contents, as described
  at https://source.android.com/devices/tech/dalvik/dex-format

  Fields:
    reader: _DexReader object used to decode dex file contents.
    header: DexHeader for this dex file.
    map_list: _DexMapList object containing list of dex file contents.
    type_item_list: _TypeIdItemList containing type_id_items.
    proto_item_list: _ProtoIdItemList containing proto_id_items.
    method_item_list: _MethodIdItemList containing method_id_items.
    string_item_list: _StringItemList containing string_data_items that are
      referenced by index in other sections.
    type_list_item_list: _TypeListItemList containing _TypeListItems.
      _TypeListItems are referenced by their offsets from other dex items.
    class_def_item_list: _ClassDefItemList containing _ClassDefItems.
  """
  _CLASS_ACCESS_FLAGS = {
      0x1: 'public',
      0x2: 'private',
      0x4: 'protected',
      0x8: 'static',
      0x10: 'final',
      0x200: 'interface',
      0x400: 'abstract',
      0x1000: 'synthetic',
      0x2000: 'annotation',
      0x4000: 'enum',
  }

  def __init__(self, data):
    """Decodes dex file memory sections.

    Args:
      data: bytearray containing the contents of a dex file.
    """
    self.reader = _DexReader(data)
    self.header = self.reader.ReadHeader()
    self.map_list = _DexMapList(self.reader, self.header.map_off)
    self.type_item_list = _TypeIdItemList(self.reader, self.header.type_ids_off,
                                          self.header.type_ids_size)
    self.proto_item_list = _ProtoIdItemList(self.reader,
                                            self.header.proto_ids_off,
                                            self.header.proto_ids_size)
    self.method_item_list = _MethodIdItemList(self.reader,
                                              self.header.method_ids_off,
                                              self.header.method_ids_size)
    self.string_item_list = _StringItemList(self.reader,
                                            self.header.string_ids_off,
                                            self.header.string_ids_size)
    self.class_def_item_list = _ClassDefItemList(self.reader,
                                                 self.header.class_defs_off,
                                                 self.header.class_defs_size)

    type_list_key = _DexMapList.TYPE_TYPE_LIST
    if type_list_key in self.map_list:
      map_list_item = self.map_list[type_list_key]
      self.type_list_item_list = _TypeListItemList(self.reader,
                                                   map_list_item.offset,
                                                   map_list_item.size)
    else:
      self.type_list_item_list = _TypeListItemList(self.reader, 0, 0)
    self._type_lists_by_offset = {
        type_list.offset: type_list
        for type_list in self.type_list_item_list
    }

  def GetString(self, string_item_idx):
    string_item = self.string_item_list[string_item_idx]
    return string_item.data

  def GetTypeString(self, type_item_idx):
    type_item = self.type_item_list[type_item_idx]
    return self.GetString(type_item.descriptor_idx)

  def GetTypeListStringsByOffset(self, offset):
    if not offset:
      return ()
    type_list = self._type_lists_by_offset[offset]
    return tuple(self.GetTypeString(item.type_idx) for item in type_list)

  @staticmethod
  def ResolveClassAccessFlags(access_flags):
    return tuple(flag_string
                 for flag, flag_string in DexFile._CLASS_ACCESS_FLAGS.items()
                 if flag & access_flags)

  def IterMethodSignatureParts(self):
    """Yields the string components of dex methods in a dex file.

    Yields:
      Tuples that look like:
        (class name, return type, method name, (parameter type, ...)).
    """
    for method_item in self.method_item_list:
      class_name_string = self.GetTypeString(method_item.type_idx)
      method_name_string = self.GetString(method_item.name_idx)
      proto_item = self.proto_item_list[method_item.proto_idx]
      return_type_string = self.GetTypeString(proto_item.return_type_idx)
      parameter_types = self.GetTypeListStringsByOffset(
          proto_item.parameters_off)
      yield (class_name_string, return_type_string, method_name_string,
             parameter_types)

  def __repr__(self):
    items = [
        self.header,
        self.map_list,
        self.type_item_list,
        self.proto_item_list,
        self.method_item_list,
        self.string_item_list,
        self.type_list_item_list,
        self.class_def_item_list,
    ]
    return '\n'.join(str(item) for item in items)


class _DumpCommand:
  def __init__(self, dexfile):
    self._dexfile = dexfile

  def Run(self):
    raise NotImplementedError()


class _DumpMethods(_DumpCommand):
  def Run(self):
    for parts in self._dexfile.IterMethodSignatureParts():
      class_type, return_type, method_name, parameter_types = parts
      print('{} {} (return type={}, parameters={})'.format(
          class_type, method_name, return_type, parameter_types))


class _DumpStrings(_DumpCommand):
  def Run(self):
    for string_item in self._dexfile.string_item_list:
      # Some strings are likely to be non-ascii (vs. methods/classes).
      print(string_item.data.encode('utf-8'))


class _DumpClasses(_DumpCommand):
  def Run(self):
    for class_item in self._dexfile.class_def_item_list:
      class_string = self._dexfile.GetTypeString(class_item.class_idx)
      superclass_string = self._dexfile.GetTypeString(class_item.superclass_idx)
      interfaces = self._dexfile.GetTypeListStringsByOffset(
          class_item.interfaces_off)
      access_flags = DexFile.ResolveClassAccessFlags(class_item.access_flags)
      print('{} (superclass={}, interfaces={}, access_flags={})'.format(
          class_string, superclass_string, interfaces, access_flags))


class _DumpSummary(_DumpCommand):
  def Run(self):
    print(self._dexfile)


def _DumpDexItems(dexfile_data, name, item):
  dexfile = DexFile(bytearray(dexfile_data))
  print('dex_parser: Dumping {} for {}'.format(item, name))
  cmds = {
      'summary': _DumpSummary,
      'methods': _DumpMethods,
      'strings': _DumpStrings,
      'classes': _DumpClasses,
  }
  try:
    cmds[item](dexfile).Run()
  except IOError as e:
    if e.errno == errno.EPIPE:
      # Assume we're piping to "less", do nothing.
      pass


def main():
  parser = argparse.ArgumentParser(description='Dump dex contents to stdout.')
  parser.add_argument('input',
                      help='Input (.dex, .jar, .zip, .aab, .apk) file path.')
  parser.add_argument('item',
                      choices=('methods', 'strings', 'classes', 'summary'),
                      help='Item to dump',
                      nargs='?',
                      default='summary')
  args = parser.parse_args()

  if os.path.splitext(args.input)[1] in ('.apk', '.jar', '.zip', '.aab'):
    with zipfile.ZipFile(args.input) as z:
      dex_file_paths = [
          f for f in z.namelist() if re.match(r'.*classes[0-9]*\.dex$', f)
      ]
      if not dex_file_paths:
        print('Error: {} does not contain any classes.dex files'.format(
            args.input))
        sys.exit(1)

      for path in dex_file_paths:
        _DumpDexItems(z.read(path), path, args.item)

  else:
    with open(args.input, 'rb') as f:
      _DumpDexItems(f.read(), args.input, args.item)


if __name__ == '__main__':
  main()
