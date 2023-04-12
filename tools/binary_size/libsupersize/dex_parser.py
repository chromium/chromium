#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for optimistically parsing DEX files.

This file is not meant to provide a generic tool for analyzing DEX files.
A DexFile class that exposes access to several memory items in the DEX format
is provided, but it does not include error handling or validation.
"""

import abc
import argparse
import collections
import errno
import functools
import os
import re
import struct
import sys
import zipfile

import dalvik_bytecode
import stream_reader

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


class TypeCode:
  LOOKUP = {}

  def __init__(self, name, value):
    assert value not in TypeCode.LOOKUP
    self.name = name
    self.value = value
    TypeCode.LOOKUP[value] = self

# Full list of type codes:
# https://source.android.com/devices/tech/dalvik/dex-format#type-codes
_TYPE_HEADER_ITEM = TypeCode('HEADER', 0x0000)
_TYPE_STRING_ID_ITEM = TypeCode('STRING_ID', 0x0001)
_TYPE_TYPE_ID_ITEM = TypeCode('TYPE_ID', 0x0002)
_TYPE_PROTO_ID_ITEM = TypeCode('PROTO_ID', 0x0003)
_TYPE_FIELD_ID_ITEM = TypeCode('FIELD_ID', 0x0004)
_TYPE_METHOD_ID_ITEM = TypeCode('METHOD_ID', 0x0005)
_TYPE_CLASS_DEF_ITEM = TypeCode('CLASS_DEF', 0x0006)
_TYPE_CALL_SITE_ID_ITEM = TypeCode('CALL_SITE_ID', 0x0007)
_TYPE_METHOD_HANDLE_ITEM = TypeCode('METHOD_HANDLE', 0x0008)
_TYPE_MAP_LIST = TypeCode('MAP', 0x1000)
_TYPE_TYPE_LIST = TypeCode('TYPE', 0x1001)
_TYPE_ANNOTATION_SET_REF_LIST = TypeCode('ANNOTATION_SET_REF', 0x1002)
_TYPE_ANNOTATION_SET_ITEM = TypeCode('ANNOTATION_SET', 0x1003)
_TYPE_CLASS_DATA_ITEM = TypeCode('CLASS_DATA', 0x2000)
_TYPE_CODE_ITEM = TypeCode('CODE', 0x2001)
_TYPE_STRING_DATA_ITEM = TypeCode('STRING_DATA', 0x2002)
_TYPE_DEBUG_INFO_ITEM = TypeCode('DEBUG_INFO', 0x2003)
_TYPE_ANNOTATION_ITEM = TypeCode('ANNOTATION', 0x2004)
_TYPE_ENCODED_ARRAY_ITEM = TypeCode('ENCODED_ARRAY', 0x2005)
_TYPE_ANNOTATIONS_DIRECTORY_ITEM = TypeCode('ANNOTATIONS_DIRECTORY', 0x2006)
_TYPE_HIDDENAPI_CLASS_DATA_ITEM = TypeCode('HIDDENAPI_CLASS_DATA', 0xF000)

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

DexHeader = collections.namedtuple('DexHeader',
                                   ','.join(t[0] for t in _DEX_HEADER_FMT))

# Simple memory items.
_StringDataItem = collections.namedtuple('StringDataItem',
                                         'utf16_size,data,byte_size')
_TypeIdItem = collections.namedtuple('TypeIdItem', 'descriptor_idx')
_ProtoIdItem = collections.namedtuple(
    'ProtoIdItem', 'shorty_idx,return_type_idx,parameters_off')
_FieldIdItem = collections.namedtuple('FieldIdItem',
                                      'class_idx,type_idx,name_idx')
_MethodIdItem = collections.namedtuple('MethodIdItem',
                                       'type_idx,proto_idx,name_idx')
_ClassDefItem = collections.namedtuple(
    'ClassDefItem',
    'class_idx,access_flags,superclass_idx,interfaces_off,source_file_idx,'
    'annotations_off,class_data_off,static_values_off')

_ClassDataItem = collections.namedtuple(
    'ClassDataItem',
    'offset,static_fields_size,instance_fields_size,direct_methods_size,'
    'virtual_methods_size,static_fields,instance_fields,direct_methods,'
    'virtual_methods')
_EncodedField = collections.namedtuple('EncodedField', 'field_idx,access_flags')
_EncodedMethod = collections.namedtuple('EncodedMethod',
                                        'method_idx,access_flags,code_off')

_TypeItem = collections.namedtuple('TypeItem', 'type_idx')
_CodeItem = collections.namedtuple(
    'CodeItem',
    'offset,registers_size,ins_size,outs_size,tries_size,debug_info_off,'
    'insns_size,insns,tries,handlers')
_TryItem = collections.namedtuple('TryItem',
                                  'start_addr,insn_count,handler_off')
_EncodedCatchHandlerList = collections.namedtuple('_EncodedCatchHandlerList',
                                                  'size,list')
_EncodedCatchHandler = collections.namedtuple('_EncodedCatchHandler',
                                              'size,handlers,catch_all_addr')
_EncodedTypeAddrPair = collections.namedtuple('_EncodedTypeAddrPair',
                                              'type_idx,addr')


class _DexReader(stream_reader.StreamReader):
  def NextDexHeader(self):
    header_fmt = '<' + ''.join(t[1] for t in _DEX_HEADER_FMT)
    return DexHeader._make(self.NextStruct(header_fmt))

  def NextClassDataItem(self):
    offset = self.Tell()
    static_fields_size = self.NextULeb128()
    instance_fields_size = self.NextULeb128()
    direct_methods_size = self.NextULeb128()
    virtual_methods_size = self.NextULeb128()
    static_fields = self.NextEncodedFieldList(static_fields_size)
    instance_fields = self.NextEncodedFieldList(instance_fields_size)
    direct_methods = self.NextEncodedMethodList(direct_methods_size)
    virtual_methods = self.NextEncodedMethodList(virtual_methods_size)
    return _ClassDataItem(offset, static_fields_size, instance_fields_size,
                          direct_methods_size, virtual_methods_size,
                          static_fields, instance_fields, direct_methods,
                          virtual_methods)

  def NextEncodedFieldList(self, count):
    ret = []
    field_idx = 0
    for _ in range(count):
      field_idx += self.NextULeb128()
      ret.append(_EncodedField(field_idx, self.NextULeb128()))
    return ret

  def NextEncodedMethodList(self, count):
    ret = []
    method_idx = 0
    for _ in range(count):
      method_idx += self.NextULeb128()
      ret.append(
          _EncodedMethod(method_idx, self.NextULeb128(), self.NextULeb128()))
    return ret

  def NextCodeItem(self):
    item_offset = self.Tell()
    registers_size = self.NextUShort()
    ins_size = self.NextUShort()
    outs_size = self.NextUShort()
    tries_size = self.NextUShort()
    debug_info_off = self.NextUInt()
    insns_size = self.NextUInt()
    insns = self.NextBytes(insns_size * 2)
    if tries_size > 0:
      self.AlignUpTo(4)
    tries = [self.NextTryItem() for _ in range(tries_size)]
    handlers = None
    if tries_size != 0:
      handlers = self.NextEncodedCatchHandlerList()
    self.AlignUpTo(4)
    return _CodeItem(item_offset, registers_size, ins_size, outs_size,
                     tries_size, debug_info_off, insns_size, insns, tries,
                     handlers)

  def NextTryItem(self):
    return _TryItem(self.NextUInt(), self.NextUShort(), self.NextUShort())

  def NextEncodedCatchHandlerList(self):
    size = self.NextULeb128()
    handler_list = [self.NextEncodedCatchHandler() for _ in range(size)]
    return _EncodedCatchHandlerList(size, handler_list)

  def NextEncodedCatchHandler(self):
    size = self.NextSLeb128()
    handlers = [self.NextEncodedTypeAddrPair() for _ in range(abs(size))]
    catch_all_addr = self.NextULeb128() if size <= 0 else None
    return _EncodedCatchHandler(size, handlers, catch_all_addr)

  def NextEncodedTypeAddrPair(self):
    return _EncodedTypeAddrPair(self.NextULeb128(), self.NextULeb128())


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
    if self.size > 0:
      reader.Seek(first_item_offset or offset)
      self._items = reader.NextList(size, factory)
    else:
      self._items = []
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


class _StringItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)
    string_data_item_offsets = iter([reader.NextUInt() for _ in range(size)])

    def factory(x):
      start_pos = next(string_data_item_offsets)
      x.Seek(start_pos)
      string = x.NextString()
      byte_size = x.Tell() - start_pos
      return _StringDataItem(len(string), string, byte_size)

    super().__init__(reader, offset, size, factory)


class _TypeIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = lambda x: _TypeIdItem(x.NextUInt())
    super().__init__(reader, offset, size, factory)


class _ProtoIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = lambda x: _ProtoIdItem(x.NextUInt(), x.NextUInt(), x.NextUInt())
    super().__init__(reader, offset, size, factory)


class _FieldIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = (
        lambda x: _FieldIdItem(x.NextUShort(), x.NextUShort(), x.NextUInt()))
    super().__init__(reader, offset, size, factory)


class _MethodIdItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    factory = (
        lambda x: _MethodIdItem(x.NextUShort(), x.NextUShort(), x.NextUInt()))
    super().__init__(reader, offset, size, factory)


class _ClassDefItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)

    def factory(x):
      return _ClassDefItem(*(x.NextUInt()
                             for _ in range(len(_ClassDefItem._fields))))

    super().__init__(reader, offset, size, factory)


class _ClassDataItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)
    super().__init__(reader, offset, size, lambda x: x.NextClassDataItem())


class _TypeListItem(_MemoryItemList):
  def __init__(self, reader):
    offset = reader.Tell()
    size = reader.NextUInt()
    factory = lambda x: _TypeItem(x.NextUShort())
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


class _CodeItemList(_MemoryItemList):
  def __init__(self, reader, offset, size):
    reader.Seek(offset)
    super().__init__(reader, offset, size, lambda x: x.NextCodeItem())


class _DexMapItem:
  def __init__(self, reader):
    if reader:
      self.type = reader.NextUShort()
      reader.NextUShort()  # Unused.
      self.size = reader.NextUInt()
      self.offset = reader.NextUInt()
    else:
      self.type = None
      self.size = 0
      self.offset = 0

  def GetName(self):
    type_code = TypeCode.LOOKUP.get(self.type)
    return type_code.name if type_code else ('(Type code %04x)' % self.type)

  def __repr__(self):
    return '_DexMapItem(type={}, size={}, offset={:#x})'.format(
        self.type, self.size, self.offset)


class _DexMapList:
  def __init__(self, reader, offset):
    self._map = {}
    reader.Seek(offset)
    self._size = reader.NextUInt()
    for _ in range(self._size):
      item = _DexMapItem(reader)
      self._map[item.type] = item

  def get(self, key):
    return self._map.get(key)

  def GetMapItemsSortedByOffset(self):
    return sorted(self._map.values(), key=lambda x: x.offset)

  def __repr__(self):
    return '_DexMapList(size={}, items={})'.format(self._size, self._map)


class DexFile:
  """Represents a single DEX file.

  Parses and exposes access to DEX file structure and contents, as described
  at https://source.android.com/devices/tech/dalvik/dex-format

  Fields:
    reader: _DexReader object used to decode DEX file contents.
    header: DexHeader for this DEX file.
    map_list: _DexMapList object containing list of DEX file contents.
    string_data_item_list: _StringItemList containing _StringDataItems that are
      referenced by index in other sections.
    type_id_item_list: _TypeIdItemList containing _TypeIdItems.
    proto_id_item_list: _ProtoIdItemList containing _ProtoIdItems.
    field_id_item_list: _FieldIdItemList containing _FieldIdItems.
    method_id_item_list: _MethodIdItemList containing _MethodIdItems.
    type_list_item_list: _TypeListItemList containing _TypeListItems.
      _TypeListItems are referenced by their offsets from other DEX items.
    class_def_item_list: _ClassDefItemList containing _ClassDefItems.
    class_data_item_list: _ClassDataItemList containing _ClassDataItems.
    code_item_list: _CodeItemList containing _CodeItems.
  """

  def __init__(self, data, is_eager=False):
    """Decodes dex file memory sections.

    Args:
      data: bytearray containing the contents of a DEX file.
      is_eager: Whether to parse |data| fully (slower) instead of lazy parsing.
    """
    self.size = len(data)
    self.reader = _DexReader(data)
    self.header = self.reader.NextDexHeader()
    self.map_list = _DexMapList(self.reader, self.header.map_off)

    # Data that require nontrivial processing are lazily initialized by default,
    # with support for eager initialization if desired.
    if is_eager:
      _ = self.string_data_item_list
      _ = self.type_id_item_list
      _ = self.proto_id_item_list
      _ = self.field_id_item_list
      _ = self.method_id_item_list
      _ = self.class_def_item_list
      _ = self.type_list_item_list
      _ = self.class_data_item_list
      _ = self.code_item_list
      _ = self._type_lists_by_offset
      _ = self._class_data_item_by_offset
      _ = self._code_item_by_offset

  @property
  @functools.lru_cache
  def string_data_item_list(self):
    return _StringItemList(self.reader, self.header.string_ids_off,
                           self.header.string_ids_size)

  @property
  @functools.lru_cache
  def type_id_item_list(self):
    return _TypeIdItemList(self.reader, self.header.type_ids_off,
                           self.header.type_ids_size)

  @property
  @functools.lru_cache
  def proto_id_item_list(self):
    return _ProtoIdItemList(self.reader, self.header.proto_ids_off,
                            self.header.proto_ids_size)

  @property
  @functools.lru_cache
  def field_id_item_list(self):
    return _FieldIdItemList(self.reader, self.header.field_ids_off,
                            self.header.field_ids_size)

  @property
  @functools.lru_cache
  def method_id_item_list(self):
    return _MethodIdItemList(self.reader, self.header.method_ids_off,
                             self.header.method_ids_size)

  @property
  @functools.lru_cache
  def class_def_item_list(self):
    return _ClassDefItemList(self.reader, self.header.class_defs_off,
                             self.header.class_defs_size)

  @property
  @functools.lru_cache
  def type_list_item_list(self):
    item = self.map_list.get(_TYPE_TYPE_LIST.value) or _DexMapItem(None)
    return _TypeListItemList(self.reader, item.offset, item.size)

  @property
  @functools.lru_cache
  def class_data_item_list(self):
    item = self.map_list.get(_TYPE_CLASS_DATA_ITEM.value) or _DexMapItem(None)
    return _ClassDataItemList(self.reader, item.offset, item.size)

  @property
  @functools.lru_cache
  def code_item_list(self):
    item = self.map_list.get(_TYPE_CODE_ITEM.value) or _DexMapItem(None)
    return _CodeItemList(self.reader, item.offset, item.size)

  @property
  @functools.lru_cache
  def _type_lists_by_offset(self):
    return {
        type_list.offset: type_list
        for type_list in self.type_list_item_list
    }

  @property
  @functools.lru_cache
  def _class_data_item_by_offset(self):
    return {
        class_data_item.offset: class_data_item
        for class_data_item in self.class_data_item_list
    }

  @property
  @functools.lru_cache
  def _code_item_by_offset(self):
    return {code_item.offset: code_item for code_item in self.code_item_list}

  def ComputeMapItemSizes(self):
    """Returns map item offsets and sizes.

    Returns: A list of dicts containing offsets and sizes of each DEX map item
      (i.e., DEX section). Fields:
      * name: Simplified item type code name (FOO = TYPE_FOO_ITEM).
      * offset: Byte offset of map item.
      * size: Number of elements in map item.
      * byte_size: Number of bytes spanned by map item, estimated by subtracting
        element offsets (and file length for the last map item). Therefore this
        also includes alignment paddings.
    """
    map_items = self.map_list.GetMapItemsSortedByOffset()
    n = len(map_items)
    ret = []
    for i in range(n):
      end = self.size if i + 1 >= n else map_items[i + 1].offset
      item = map_items[i]
      ret.append({
          'name': item.GetName(),
          'offset': item.offset,
          'size': item.size,
          'byte_size': end - item.offset
      })
    return ret

  def GetString(self, string_idx):
    string_data_item = self.string_data_item_list[string_idx]
    return string_data_item.data

  def GetTypeString(self, type_idx):
    type_id_item = self.type_id_item_list[type_idx]
    return self.GetString(type_id_item.descriptor_idx)

  def GetTypeListStringsByOffset(self, offset):
    if not offset:
      return ()
    type_list = self._type_lists_by_offset[offset]
    return tuple(self.GetTypeString(item.type_idx) for item in type_list)

  def GetClassDataItemByOffset(self, offset):
    return self._class_data_item_by_offset.get(offset)

  def GetCodeItemByOffset(self, offset):
    return self._code_item_by_offset.get(offset)

  def IterAllStringIdsUsedByCodeItem(self, code_item):
    if not code_item:
      return
    for bytecode in dalvik_bytecode.Split(code_item.insns):
      if bytecode[0] in (0x1a, 0x1b):
        # 1a 21c  const-string vAA, string@BBBB
        # 1b 31c  const-string/jumbo vAA, string@BBBBBBBB
        fmt = '<H' if bytecode[0] == 0x1a else '<L'
        yield struct.unpack_from(fmt, bytecode, 2)[0]

  @staticmethod
  def ResolveClassAccessFlags(access_flags):
    return tuple(flag_string
                 for flag, flag_string in _CLASS_ACCESS_FLAGS.items()
                 if flag & access_flags)

  def IterMethodSignatureParts(self):
    """Yields the string components of DEX methods in a DEX file.

    Yields:
      Tuples that look like:
        (class name, return type, method name, (parameter type, ...)).
    """
    for method_id_item in self.method_id_item_list:
      class_name = self.GetTypeString(method_id_item.type_idx)
      method_name = self.GetString(method_id_item.name_idx)
      proto_id_item = self.proto_id_item_list[method_id_item.proto_idx]
      return_type_name = self.GetTypeString(proto_id_item.return_type_idx)
      parameter_types = self.GetTypeListStringsByOffset(
          proto_id_item.parameters_off)
      yield (class_name, return_type_name, method_name, parameter_types)

  def __repr__(self):
    items = [
        self.header,
        self.map_list,
        self.string_data_item_list,
        self.type_id_item_list,
        self.proto_id_item_list,
        self.field_id_item_list,
        self.method_id_item_list,
        self.type_list_item_list,
        self.class_def_item_list,
        self.class_data_item_list,
        self.code_item_list,
    ]
    return '\n'.join(str(item) for item in items)


class _DumpCommand:
  def __init__(self, dexfile):
    self._dexfile = dexfile

  @abc.abstractmethod
  def Run(self):
    pass


class _DumpSummary(_DumpCommand):
  def Run(self):
    print(self._dexfile)


class _DumpStrings(_DumpCommand):
  def Run(self):
    for (i, string_data_item) in enumerate(self._dexfile.string_data_item_list):
      # Some strings are likely to be non-ascii (vs. methods/classes).
      s = string_data_item.data
      rep_str = (repr(s) if s.isprintable() else s.encode(
          'utf-8', errors='surrogatepass'))
      print('string(%08X): %s' % (i, rep_str))


class _DumpMap(_DumpCommand):
  def Run(self):
    for item_info in self._dexfile.ComputeMapItemSizes():
      print(item_info)


class _DumpFields(_DumpCommand):
  def Run(self):
    dexfile = self._dexfile
    for (i, field_id_item) in enumerate(dexfile.field_id_item_list):
      class_name = dexfile.GetTypeString(field_id_item.class_idx)
      type_name = dexfile.GetTypeString(field_id_item.type_idx)
      name = dexfile.GetString(field_id_item.name_idx)
      print('field(%08x): (class=%s, name=%s, type=%s)' %
            (i, class_name, name, type_name))


class _DumpMethods(_DumpCommand):
  def Run(self):
    for (i, parts) in enumerate(self._dexfile.IterMethodSignatureParts()):
      class_name, return_type_name, method_name, parameter_types = parts
      print('method(%08x): (class=%s, name=%s, return_type=%s, params=%s)' %
            (i, class_name, method_name, return_type_name, parameter_types))


class _DumpClasses(_DumpCommand):
  def Run(self):
    dexfile = self._dexfile
    fmt = ('class(%08x): (name=%s, superclass=%s, interfaces=%s, ' +
           'access_flags=%s)')
    for (i, class_item) in enumerate(dexfile.class_def_item_list):
      name = dexfile.GetTypeString(class_item.class_idx)
      superclass_name = dexfile.GetTypeString(class_item.superclass_idx)
      interfaces = dexfile.GetTypeListStringsByOffset(class_item.interfaces_off)
      access_flags = DexFile.ResolveClassAccessFlags(class_item.access_flags)
      print(fmt % (i, name, superclass_name, interfaces, access_flags))


class _DumpCodes(_DumpCommand):
  def Run(self):
    dexfile = self._dexfile
    total_insns_bytes = 0
    total_insns_count = 0
    total_code_bytes = 0
    total_tries_count = 0
    insns_popu = [0] * 256
    attrs = [
        'registers_size', 'ins_size', 'outs_size', 'tries_size', 'insns_size'
    ]
    for i, code_item in enumerate(dexfile.code_item_list):
      total_insns_bytes += len(code_item.insns)
      total_tries_count += code_item.tries_size
      insns_count = 0
      actual_insns_bytes = 0
      for bytecode in dalvik_bytecode.Split(code_item.insns):
        insns_popu[bytecode[0]] += 1
        insns_count += 1
        actual_insns_bytes += len(bytecode)
      total_insns_count += insns_count
      total_code_bytes += actual_insns_bytes
      details = ', '.join('{}={}'.format(a, getattr(code_item, a))
                          for a in attrs if getattr(code_item, a) > 0)
      print('code(%08x) o:%08X: (%s)' % (i, code_item.offset, details))
    total_data_bytes = total_insns_bytes - total_code_bytes

    print('\n*** Summary ***')
    print('Code item count:         %d' % len(dexfile.code_item_list))
    print('Total instruction count: %d' % total_insns_count)
    print('Total instruction bytes: %d' % total_insns_bytes)
    print('  Code: %d' % total_code_bytes)
    print('  Data: %d' % total_data_bytes)
    print('Total tries count:       %d' % total_tries_count)
    print('Total tries bytes:       %d' % (total_tries_count * (4 + 2 + 2)))


def _DumpDexItems(dexfile_data, name, item, is_eager):
  dexfile = DexFile(data=bytearray(dexfile_data), is_eager=is_eager)
  print('dex_parser: Dumping {} for {}'.format(item, name))
  cmds = {
      'summary': _DumpSummary,
      'map': _DumpMap,
      'strings': _DumpStrings,
      'fields': _DumpFields,
      'methods': _DumpMethods,
      'classes': _DumpClasses,
      'codes': _DumpCodes,
  }
  try:
    cmds[item](dexfile).Run()
  except IOError as e:
    if e.errno == errno.EPIPE:
      # Assume we're piping to "less", do nothing.
      pass


def main():
  parser = argparse.ArgumentParser(description='Dump DEX contents to stdout.')
  parser.add_argument('input',
                      help='Input (.dex, .jar, .zip, .aab, .apk) file path.')
  parser.add_argument('item',
                      choices=('summary', 'map', 'strings', 'fields', 'methods',
                               'classes', 'codes'),
                      help='Item to dump',
                      nargs='?',
                      default='summary')
  parser.add_argument('--eager', action='store_true')
  args = parser.parse_args()

  if args.item == 'summary':
    args.eager = True

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
        _DumpDexItems(z.read(path), path, args.item, args.eager)

  else:
    with open(args.input, 'rb') as f:
      _DumpDexItems(f.read(), args.input, args.item, args.eager)


if __name__ == '__main__':
  main()
