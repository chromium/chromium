#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for ARSC file parsing.

This file provides tools to performs shallow parsing for binary size analysis at
chunk level, without comprehensive error detection.
ARSC file format are extracted from:
https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/include/androidfw/ResourceTypes.h
https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/ResourceTypes.cpp
"""

import argparse
import collections
import dataclasses
import logging
import functools
import os
import struct
import sys
import zipfile

import stream_reader

# List of all possible ARSC chunk types (|type| field in ArscChunk).

_RES_NULL_TYPE = 0x0000
_RES_STRING_POOL_TYPE = 0x0001
_RES_TABLE_TYPE = 0x0002
_RES_XML_TYPE = 0x0003
# Chunk types in _RES_XML_TYPE
_RES_XML_FIRST_CHUNK_TYPE = 0x0100
_RES_XML_START_NAMESPACE_TYPE = 0x0100
_RES_XML_END_NAMESPACE_TYPE = 0x0101
_RES_XML_START_ELEMENT_TYPE = 0x0102
_RES_XML_END_ELEMENT_TYPE = 0x0103
_RES_XML_CDATA_TYPE = 0x0104
_RES_XML_LAST_CHUNK_TYPE = 0x017f
# This contains a uint32_t array mapping strings in the string pool back to
# resource identifiers.  It is optional.
_RES_XML_RESOURCE_MAP_TYPE = 0x0180
# Chunk types in _RES_TABLE_TYPE
_RES_TABLE_PACKAGE_TYPE = 0x0200
_RES_TABLE_TYPE_TYPE = 0x0201
_RES_TABLE_TYPE_SPEC_TYPE = 0x0202
_RES_TABLE_LIBRARY_TYPE = 0x0203
_RES_TABLE_OVERLAYABLE_TYPE = 0x0204
_RES_TABLE_OVERLAYABLE_POLICY_TYPE = 0x0205
_RES_TABLE_STAGED_ALIAS_TYPE = 0x0206

_StringInfo = collections.namedtuple('StringInfo', 'str_size,enc_size,data')


class _ArscStreamReader(stream_reader.StreamReader):
  def Clone(self):
    ret = _ArscStreamReader(self._data)
    ret.Seek(self.Tell())
    return ret

  def NextArscEncodedLengthUtf8(self):
    byte1 = self.NextUByte()
    return ((byte1 & 0x7F) << 8) | self.NextUByte() if byte1 & 0x80 else byte1

  def NextArscEncodedLengthWide(self):
    short1 = self.NextUShort()
    return (((short1 & 0x7FFF) << 16) | self.NextUShort() if short1
            & 0x8000 else short1)

  def NextCString(self, n):
    t = self.NextBytes(n)
    term = t.find(0)
    t = t if term < 0 else t[:term]
    return t.decode('latin1')

  def PeekArscHeaderType(self):
    pos = self.Tell()
    ret = self.NextUShort()
    self.Seek(pos)
    return ret

  @functools.lru_cache
  def GetArscResTypeToClassMap(self):
    """Returns a lookup table to map chunk type to class.

    Requires all classes in this file to be parsed, before calling.
    """
    def MakeGeneric(type_name):
      return lambda reader, parent: ArscGeneric(type_name, reader, parent)

    return {
        _RES_STRING_POOL_TYPE: ArscStringPool,
        _RES_TABLE_TYPE: ArscResTable,
        _RES_XML_TYPE: MakeGeneric('XML'),
        _RES_XML_FIRST_CHUNK_TYPE: MakeGeneric('XML_FIRST_CHUNK'),
        _RES_XML_START_NAMESPACE_TYPE: MakeGeneric('XML_START_NAMESPACE'),
        _RES_XML_END_NAMESPACE_TYPE: MakeGeneric('XML_END_NAMESPACE'),
        _RES_XML_START_ELEMENT_TYPE: MakeGeneric('XML_START_ELEMENT'),
        _RES_XML_END_ELEMENT_TYPE: MakeGeneric('XML_END_ELEMENT'),
        _RES_XML_CDATA_TYPE: MakeGeneric('XML_CDATA'),
        _RES_XML_LAST_CHUNK_TYPE: MakeGeneric('XML_LAST_CHUNK'),
        _RES_XML_RESOURCE_MAP_TYPE: MakeGeneric('XML_RESOURCE_MAP'),
        _RES_TABLE_PACKAGE_TYPE: ArscResTablePackage,
        _RES_TABLE_TYPE_TYPE: ArscResTableType,
        _RES_TABLE_TYPE_SPEC_TYPE: ArscResTableTypeSpec,
        _RES_TABLE_LIBRARY_TYPE: MakeGeneric('LIBRARY'),
        _RES_TABLE_OVERLAYABLE_TYPE: MakeGeneric('OVERLAYABLE'),
        _RES_TABLE_OVERLAYABLE_POLICY_TYPE: MakeGeneric('OVERLAYABLE_POLICY'),
        _RES_TABLE_STAGED_ALIAS_TYPE: MakeGeneric('STAGED_ALIAS'),
    }

  def NextArscChunk(self, parent=None):
    chunk_type = self.PeekArscHeaderType()
    arsc_class = self.GetArscResTypeToClassMap().get(chunk_type) or ArscChunk
    chunk = arsc_class(self, parent=parent)
    self.Seek(chunk.end_addr)
    return chunk


def _SplitBits(value, *widths):
  for width in widths:
    yield value & ((1 << width) - 1)
    value >>= width


class ResTableConfig:
  """Structure to specify |config| in ArscResTableType."""
  def __init__(self, reader):
    self.size = reader.NextUInt()
    assert self.size == 64
    # imsi:32 -> {mcc:16, mnc:16} (ordered LSB first).
    self.imsi = reader.NextUInt()
    # locale:32 -> {languages[2]:8, country[2]:8}.
    self.locale = reader.NextUInt()
    # screen_type:32 -> {orientation:8, touchscreen:8, density:16}.
    self.screen_type = reader.NextUInt()
    # input:32 -> {keyboard:8, navigation:8, input_flags:8, input_pad0:8}.
    #   input_flags:8 -> {keys:2, nav:2, unused1:4}.
    self.input = reader.NextUInt()
    # screen_size:32 -> {screen_width:16, screen_height:16}.
    self.screen_size = reader.NextUInt()
    # version:32 -> {sdk_version:16, minor_version:16}.
    self.version = reader.NextUInt()
    # screen_config:32 -> {screen_layout:8, ui_mode:8,
    #                      smallest_screen_width_dp:16}.
    #   screen_layout:8 -> {screen_layout_size:4, screen_long:2, layout_dir:2}.
    #   ui_mode:8 -> {ui_mode_type:4, ui_mode_night:2, unused2:2}.
    self.screen_config = reader.NextUInt()
    # screen_size_dp:32 -> {screen_width_dp:16, screen_height_dp:16}.
    self.screen_size_dp = reader.NextUInt()
    self.locale_script = reader.NextCString(4)
    self.locale_variant = reader.NextCString(8)
    # screen_config2:32 -> {screen_layout2:8, color_mode:8,
    #                       screen_config_pad2:16}.
    #   screen_layout2:8 -> {screen_round:2, unused3:6}.
    #   color_mode:8 -> {wide_color_gamut:2, hdr:2, unused4:4}.
    self.screen_config2 = reader.NextUInt()
    self.locale_script_was_computed = bool(reader.NextUByte())
    self.locale_numbering_system = reader.NextCString(8)
    # Skip 3 bytes to account for struct padding: Total size is 64 bytes.
    reader.Skip(3)

  def EmitField(self, v, start, width, fmt, lookup=None):
    """Formats and yields bit range from an integer if non-0."""
    bits = (v >> start) & ((1 << width) - 1)
    if bits != 0:
      yield (lookup and lookup.get(bits)) or fmt % bits

  def DecodeLanguageOrRegion(self, code, base):
    if code & 0x8000 == 0:
      # Two-letter encoding: (MSB) 0sssssss 0fffffff for first & second letters.
      return (chr(code & 0x7f) + chr(code >> 8)).rstrip('\0')
    # Three-letter encoding: (MSB) 1tttttss sssfffff.
    return ''.join(chr(base + ((code >> i) & 0x1f)) for i in range(3))

  def EmitLocaleString(self):
    """Emits locale data as formatted string if non-0."""
    if self.locale == 0:
      return
    language, country = _SplitBits(self.locale, 16, 16)
    script_was_provided = (self.locale_script
                           and not self.locale_script_was_computed)
    ret = self.DecodeLanguageOrRegion(language, ord('a'))
    if (not script_was_provided and not self.locale_variant
        and not self.locale_numbering_system):  # Legacy format.
      if country != 0:
        ret += '-r' + self.DecodeLanguageOrRegion(country, ord('0'))
    else:
      ret = 'b+' + ret
      if script_was_provided:
        ret += '+' + self.locale_script
      if country != 0:
        ret += '+' + self.DecodeLanguageOrRegion(country, ord('0'))
      if self.locale_variant:
        ret += '+' + self.locale_variant
      if self.locale_numbering_system:
        ret += '+u+nu+' + self.locale_numbering_system
    yield ret

  def EmitAllTokens(self):
    """Emits all non-0 data as string tokens.

    The formatting follows ResTable_config::toString(). Typically each value is
    extracted as bit ranges in some member variable. If a value matches a
    predefined constant (e.g., 4 = "xlarge") then the token is taken as the
    string. Otherwise a fallback (e.g., "screenLayoutSize=5") is used instead.
    """
    # yapf: disable
    # imsi -> mcc
    yield from self.EmitField(self.imsi, 0, 16, 'mcc%d')
    # imsi -> mnc
    yield from self.EmitField(self.imsi, 16, 16, 'mnc%d')
    yield from self.EmitLocaleString()
    # screen_config -> screen_layout -> layout_dir
    yield from self.EmitField(self.screen_config, 6, 2, 'layoutDir=%d', {
      1: 'ldltr',  # LAYOUTDIR_LTR
      2: 'ldrtl',  # LAYOUTDIR_RTL
    })
    # screen_config -> smallest_screen_width_dp
    yield from self.EmitField(self.screen_config, 16, 16, 'sw%ddp')
    # screen_size_dp -> screen_width_dp
    yield from self.EmitField(self.screen_size_dp, 0, 16, 'w%ddp')
    # screen_size_dp -> screen_height_dp
    yield from self.EmitField(self.screen_size_dp, 16, 16, 'h%ddp')
    # screen_config -> screen_layout -> screen_layout_size
    yield from self.EmitField(self.screen_config, 0, 4, 'screenLayoutSize=%d', {
      1: 'small',  # SCREENSIZE_SMALL
      2: 'normal',  # SCREENSIZE_NORMAL
      3: 'large',  # SCREENSIZE_LARGE
      4: 'xlarge',  # SCREENSIZE_XLARGE
    })
    # screen_config -> screen_layout -> screen_long
    yield from self.EmitField(self.screen_config, 4, 2, 'screenLayoutLong=%d', {
      1: 'notling',  # SCREENLOG_NO
      2: 'long',  # SCREENLOG_YES
    })
    # screen_config2 -> screen_layout2 -> screen_round
    yield from self.EmitField(self.screen_config2, 0, 2, 'screenRound=%d', {
      1: 'notround',  # SCREENROUND_NO
      2: 'round',  # SCREENROUND_YES
    })
    # screen_config2 -> color_mode -> wide_color_gamut
    yield from self.EmitField(self.screen_config2, 8, 2, 'wideColorGamut=%d', {
      1: 'nowidecg',  # WIDE_COLOR_GAMUT_NO
      2: 'widecg',  # WIDE_COLOR_GAMUT_YES
    })
    # screen_config2 -> color_mode -> hdr
    yield from self.EmitField(self.screen_config2, 10, 2, 'hdr=%d', {
      1: 'lowdr',  # HDR_NO
      2: 'highdr',  # HDR_YES
    })
    # screen_type -> orientation
    yield from self.EmitField(self.screen_type, 0, 8, 'orientation=%d', {
      1: 'port',  # ORIENTATION_PORT
      2: 'land',  # ORIENTATION_LAND
      3: 'square',  # ORIENTATION_SQUARE
    })
    # screen_config -> ui_mode -> ui_mode_type
    yield from self.EmitField(self.screen_config, 8, 4, 'uiModeType=%d', {
      1: 'normal',  # UI_MODE_TYPE_NORMAL
      2: 'desk',  # UI_MODE_TYPE_DESK
      3: 'car',  # UI_MODE_TYPE_CAR
      4: 'television',  # UI_MODE_TYPE_TELEVISION
      5: 'appliance',  # UI_MODE_TYPE_APPLIANCE
      6: 'watch',  # UI_MODE_TYPE_WATCH
      7: 'vrheadset',  # UI_MODE_TYPE_VR_HEADSET
    })
    # screen_config -> ui_mode -> ui_mode_night
    yield from self.EmitField(self.screen_config, 12, 2, 'uiModeNight=%d', {
      1: 'notnight',  # UI_MODE_NIGHT_NO
      2: 'night',  # UI_MODE_NIGHT_YES
    })
    # screen_type -> density
    yield from self.EmitField(self.screen_type, 16, 16, '%ddpi', {
      120: 'ldpi',  # DENSITY_LOW
      160: 'mdpi',  # DENSITY_MEDIUM
      213: 'tvdpi',  # DENSITY_TV
      240: 'hdpi',  # DENSITY_HIGH
      320: 'xhdpi',  # DENSITY_XHIGH
      480: 'xxhdpi',  # DENSITY_XXHIGH
      640: 'xxxhdpi',  # DENSITY_XXXHIGH
      0xffff: 'nodpi',  # DENSITY_NONE
      0xfffe: 'anydpi',  # DENSITY_ANY
    })
    # screen_type -> touchscreen
    yield from self.EmitField(self.screen_type, 8, 8, 'touchscreen=%d', {
      1: 'notouch',  # TOUCHSCREEN_NOTOUCH
      2: 'finger',  # TOUCHSCREEN_FINGER
      3: 'stylus',  # TOUCHSCREEN_STYLUS
    })
    # input -> input_flags -> keys
    yield from self.EmitField(self.input, 16, 2, '(keys=%d)', {
      1: 'keysexposed',  # KEYSHIDDEN_NO
      2: 'keyshidden',  # KEYSHIDDEN_YES
      3: 'keyssoft',  # KEYSHIDDEN_SOFT
    })
    # input -> keyboard
    yield from self.EmitField(self.input, 0, 8, 'keyboard=%d', {
      1: 'nokeys',  # KEYBOARD_NOKEYS
      2: 'qwerty',  # KEYBOARD_QWERTY
      3: '12key',  # KEYBOARD_12KEY
    })
    # input -> input_flags -> nav
    yield from self.EmitField(self.input, 18, 2, 'inputFlagsNavHidden=%d', {
      1: 'navexposed',  # NAVHIDDEN_NO
      2: 'navhidden',  # NAVHIDDEN_YES
    })
    # input -> navigation
    yield from self.EmitField(self.input, 8, 8, 'navigation=%d', {
      1: 'nonav',  # NAVIGATION_NONAV
      2: 'dpad',  # NAVIGATION_DPAD
      3: 'trackball',  # NAVIGATION_TRACKBALL
      4: 'wheel',  # NAVIGATION_WHEEL
    })
    # yapf: enable
    if self.screen_size != 0:
      yield '%dx%d' % tuple(_SplitBits(self.screen_size, 16, 16))
    if self.version != 0:
      sdk, minor = _SplitBits(self.version, 16, 16)
      yield f'v{sdk}' + (f'.{minor}' if minor else '')

  def __str__(self):
    return '-'.join(self.EmitAllTokens())


class ArscChunk:
  """Base class for ARSC chunks, embedding hierarchy and common header.

  Fields:
    addr: Absolute start address of chunk in the ARSC file.
    parent: Reference to the parent chunk (None for root).
    children: References to the children chunks.
    type: (In header) Chunk type specified by a _RES_*_TYPE constant.
    header_size: (In header) Byte size of the header, which is |type|-dependent.
    size: (In header) Byte size of the chunk, including header.
    placeholder: Number of placeholder bytes.
  """
  def __init__(self, reader, parent):
    # Custom additions for binary size tracking.
    self.addr = reader.Tell()
    self.parent = parent
    self.children = []

    # {type, header_size, size} constitute common header, "flattened" as membmer
    # variable (instead of being wrapped into |header| var) for simplicity.
    self.type = reader.NextUShort()
    # Total header size, including 8 bytes here and |type|-specific data.
    self.header_size = reader.NextUShort()
    # Total chunk size, including header.
    self.size = reader.NextUInt()

  @property
  def payload_addr(self):
    return self.addr + self.header_size

  @property
  def end_addr(self):
    return self.addr + self.size

  @property
  def placeholder(self):
    """Returns type-dependent placeholder, overridable."""
    return 0

  def StrHelper(self, name, fields):
    depth = 0
    cur = self.parent
    while cur:
      depth += 1
      cur = cur.parent
    r = '[%08X, %08X)' % (self.addr, self.addr + self.size)
    f = ', '.join(f'{k}={v}' for (k, v) in fields.items())
    return '%s: %s%s: %s' % (r, '  ' * depth, name, f)

  def __str__(self):
    return self.StrHelper('GENERIC', {'type': self.type})

  def symbol_name(self):
    return f'GENERIC: type={self.type}'

  def labelled_children(self):
    yield from ((None, child) for child in self.children)


class ArscGeneric(ArscChunk):
  """Generic chunk containing only name."""
  def __init__(self, type_name, reader, parent):
    super().__init__(reader, parent)
    self.type_name = type_name

  def __str__(self):
    return self.StrHelper(self.type_name, {})

  def symbol_name(self):
    return self.type_name


class ArscStringPool(ArscChunk):
  """_RES_STRING_POOL_TYPE chunk for string storage.

  Fields:
    string_count: Number of strings in chunk.
  """

  # If set, the string index is sorted by the string values.
  SORTED_FLAG = 1 << 0
  # String pool is encoded in UTF-8.
  UTF8_FLAG = 1 << 8

  def __init__(self, reader, parent=None):
    super().__init__(reader, parent)
    self.string_count = reader.NextUInt()
    self.style_count = reader.NextUInt()
    self.flags = reader.NextUInt()
    self.string_start = reader.NextUInt()
    self.style_start = reader.NextUInt()

    self.is_utf8 = bool(self.flags & ArscStringPool.UTF8_FLAG)
    assert reader.Tell() == self.payload_addr

    self.role = ''  # Can be modified by parent.
    base = self.addr + self.string_start
    self.string_addrs = [
        base + reader.NextUInt() for _ in range(self.string_count)
    ]

    # Clone to enable lazy string read without polluting |reader|. This is
    # cleared when no longer needed.
    self.reader = reader.Clone()

  def __str__(self):
    return self.StrHelper('STRING_POOL', {'string_count': self.string_count})

  def symbol_name(self):
    return f'STRING_POOL: {self.role}' if self.role else 'STRING_POOL'

  @property
  @functools.lru_cache
  def string_infos(self):
    ret = []
    if self.is_utf8:
      for offset in self.string_addrs:
        self.reader.Seek(offset)
        str_size = self.reader.NextArscEncodedLengthUtf8()
        enc_size = self.reader.NextArscEncodedLengthUtf8()
        data = self.reader.NextBytes(enc_size)
        ret.append(_StringInfo(str_size, enc_size, data))
    else:
      for offset in self.string_addrs:
        self.reader.Seek(offset)
        str_size = self.reader.NextArscEncodedLengthWide()
        enc_size = str_size * 2
        data = self.reader.NextBytes(enc_size)
        ret.append(_StringInfo(str_size, enc_size, data))
    return ret

  @property
  @functools.lru_cache
  def string_items(self):
    encoding = 'utf-8' if self.is_utf8 else 'utf-16'
    ret = [
        info.data.decode(encoding, errors='surrogatepass')
        for info in self.string_infos
    ]
    self.reader = None
    return ret

  def GetString(self, idx):
    return self.string_items[idx]


class ArscResTable(ArscChunk):
  """_RES_TABLE_TYPE chunk for ARSC file root.

  Children comprise of:
    ArscStringPool
    ArscResTablePackage 1
    ...
    ArscResTablePackage n

  Fields:
    package_count: Number of ArscResTablePackage entries (typically 1).
  """
  def __init__(self, reader, parent=None):
    super().__init__(reader, parent)
    self.package_count = reader.NextUInt()
    assert reader.PeekArscHeaderType() == _RES_STRING_POOL_TYPE

    self.string_pool = reader.NextArscChunk(parent=self)
    self.string_pool.role = 'root'
    self.children.append(self.string_pool)

    # Save |cur_addr| since children chunks may not be fully parsed.
    cur_addr = self.string_pool.end_addr
    self.packages = []
    for _ in range(self.package_count):
      reader.Seek(cur_addr)
      assert reader.PeekArscHeaderType() == _RES_TABLE_PACKAGE_TYPE
      package = reader.NextArscChunk(parent=self)
      self.children.append(package)
      self.packages.append(package)
      cur_addr = self.packages[-1].end_addr

  def __str__(self):
    return self.StrHelper('TABLE', {'package_count': self.package_count})

  def symbol_name(self):
    return 'res_table'

  def labelled_children(self):
    for chunk in self.children:
      if chunk is self.string_pool:
        yield (None, chunk)
      elif isinstance(chunk, ArscResTablePackage):
        yield (chunk.name, chunk)
      else:
        yield (None, chunk)


class ArscResTablePackage(ArscChunk):
  """_RES_TABLE_PACKAGE_TYPE chunk containing a package's resources.

  Children *normally* comprise of:
    ArscStringPool: For types (small, for resource types).
    ArscStringPool: For keys (resource names)
    ArscResTableTypeSpec 1: For resource type 1.
    ArscResTableType 1.1: Config used by the type, has the same |entry_count|.
    ...
    ArscResTableType 1.n_1: Here n_1 depends on type 1.
    ArscResTableTypeSpec 2: For resource type 2.
    ArscResTableType 2.1
    ...
    ArscResTableType 2.n_2
    ...

  Fields:
    name: Android app package name for the resource.
  """
  def __init__(self, reader, parent=None):
    super().__init__(reader, parent)
    self.id = reader.NextUInt()
    assert self.id < 0x100, 'Package ID is too big'
    self.name = reader.NextBytes(256).decode('utf-16').rstrip('\0')
    self.type_strings = reader.NextUInt()
    self.last_public_type = reader.NextUInt()
    self.key_strings = reader.NextUInt()
    self.last_public_key = reader.NextUInt()
    self.type_id_offset = reader.NextUInt()
    assert reader.Tell() == self.payload_addr

    self.type_pool = None
    self.key_pool = None

    # Save |cur_addr| since children chunks may not be fully parsed.
    cur_addr = self.payload_addr
    while cur_addr < self.end_addr:
      reader.Seek(cur_addr)
      chunk = reader.NextArscChunk(parent=self)
      self.children.append(chunk)
      if isinstance(chunk, ArscStringPool):
        if self.type_pool is None:
          self.type_pool = chunk
          self.type_pool.role = 'types'
        elif self.key_pool is None:
          self.key_pool = chunk
          self.key_pool.role = 'keys'
        else:
          logging.warning('Unexpected string pool at %08X.' % t.address)
      cur_addr = chunk.end_addr

  def __str__(self):
    return self.StrHelper('PACKAGE', {'name': self.name})

  def symbol_name(self):
    return 'package'

  def labelled_children(self):
    for chunk in self.children:
      if isinstance(chunk, (ArscResTableType, ArscResTableTypeSpec)):
        yield (chunk.type_str, chunk)
      else:
        yield (None, chunk)


class ArscResTableType(ArscChunk):
  """_RES_TABLE_TYPE_TYPE chunk for resources of a common type and config.

  Following the header, the struct consinsts of a (relative) pointer table for
  resource entries, followed by resource data (not parsed). The pointer table
  can be dense or sparse.
  * Dense tables use NO_ENTRY to mark resources unavailable for |config|. These
    pointers are counted in |placeholder|.
  * Sparse tables stores sorted (index, pointer) pairs and uses binary search.
    Currently we don't support these.

  Fields:
    type_str: Name of the common type, e.g., "drawable", "layout", "string".
    config: ResTableConfig instance.
    entry_count: Number of resources contained.
  """
  NO_ENTRY = 0xffffffff
  FLAG_SPARSE = 0x01

  def __init__(self, reader, parent=None):
    super().__init__(reader, parent)
    assert parent
    assert parent.type_pool, 'Missing type string pool.'
    assert parent.key_pool, 'Missing key string pool.'
    self.id = reader.NextUByte()
    assert self.id != 0, 'ResTable_type has invalid id.'
    self.flags = reader.NextUByte()
    assert (self.flags & ArscResTableType.FLAG_SPARSE) == 0, (
        'Sparse tables are unsupported.')
    self.reserved = reader.NextUShort()
    self.entry_count = reader.NextUInt()
    self.entries_start = reader.NextUInt()
    assert self.entries_start < self.size
    self.config = ResTableConfig(reader)
    assert reader.Tell() == self.payload_addr

    self.entry_placeholder = 0
    self.type_str = parent.type_pool.GetString(self.id - 1)

    entries_start_addr = self.addr + self.entries_start
    entries_offsets = [reader.NextUInt() for _ in range(self.entry_count)]
    assert entries_start_addr >= reader.Tell()

    self.entry_placeholder += sum(4 for o in entries_offsets
                                  if o == ArscResTableType.NO_ENTRY)
    # Skip reading actual entries.

  @property
  def placeholder(self):
    return self.entry_placeholder

  def __str__(self):
    return self.StrHelper(
        'TYPE', {
            'type_str': self.type_str,
            'entry_count': self.entry_count,
            'size': self.size,
            'placeholder': self.entry_placeholder,
            'config': str(self.config),
        })

  def symbol_name(self):
    return str(self.config) or 'default'


class ArscResTableTypeSpec(ArscChunk):
  """_RES_TABLE_TYPE_SPEC_TYPE chunk for info on resource of the same type.

  The info stored is independent of config.

  Fields:
    type_str: Name of the common type, e.g., "drawable", "layout", "string".
    entry_count: Number of resources contained.
  """
  def __init__(self, reader, parent=None):
    super().__init__(reader, parent)
    assert parent
    assert parent.type_pool, 'Missing type string pool.'
    self.id = reader.NextUByte()
    assert self.id != 0, 'ResTable_typeSpec has invalid id.'
    self.res0 = reader.NextUByte()  # Must be 0.
    self.res1 = reader.NextUShort()  # Must be 0.
    self.entry_count = reader.NextUInt()
    assert self.entry_count < 0x10000, 'ResTable_typeSpec has too many entries.'

    self.type_str = parent.type_pool.GetString(self.id - 1)
    # Skip reading specs.
    reader.Seek(self.end_addr)

  def __str__(self):
    return self.StrHelper('TYPE_SPEC', {
        'type_str': self.type_str,
        'entry_count': self.entry_count
    })

  def symbol_name(self):
    return 'TYPE_SPEC'


class ArscFile:
  """Represents a single ARSC file.

  Shallowly parses an ARSC file into nested ArscChunk for binary size analysis.

  Fields:
    table: The root chunk that contains all other chunks.
  """
  def __init__(self, data):
    reader = _ArscStreamReader(data)
    assert reader.PeekArscHeaderType() == _RES_TABLE_TYPE
    self.table = reader.NextArscChunk()

  def VisitPreOrder(self):
    """Depth-first pre-order visitor of all (path, chunk).

    |path| is a string to establish context of |chunk|, consisting of non-None
    labels of ancestral and current chunks joined by '/'.
    """
    @dataclasses.dataclass
    class StackFrame:
      prev_has_label: bool
      child_iterator: object

    label_stack = []
    yield '', self.table
    stack = [StackFrame(False, self.table.labelled_children())]
    while stack:
      frame = stack[-1]
      if frame.prev_has_label:
        label_stack.pop()
        frame.prev_has_label = False
      label_and_chunk = next(frame.child_iterator, None)
      if label_and_chunk:
        label, chunk = label_and_chunk
        if label:
          label_stack.append(label)
          frame.prev_has_label = True
        yield '/'.join(label_stack), chunk
        stack.append(StackFrame(False, chunk.labelled_children()))
      else:
        stack.pop()


def _DumpArscChunks(arsc_data):
  arsc_file = ArscFile(arsc_data)
  for _, chunk in arsc_file.VisitPreOrder():
    print(str(chunk))


def main():
  parser = argparse.ArgumentParser(description='Dump ARSC contents to stdout.')
  parser.add_argument('input',
                      help='Input (.arsc, .apk, .jar, .zip) file path.')
  args = parser.parse_args()

  if os.path.splitext(args.input)[1] in ('.apk', '.jar', '.zip'):
    with zipfile.ZipFile(args.input) as z:
      arsc_file_paths = [
          f for f in z.namelist() if os.path.splitext(f)[1] == '.arsc'
      ]
      if not arsc_file_paths:
        print('Error: {} does not contain .arsc files.'.format(args.input))
        sys.exit(1)
      for path in arsc_file_paths:
        _DumpArscChunks(z.read(path))

  else:
    with open(args.input, 'rb') as fh:
      _DumpArscChunks(fh.read())


if __name__ == '__main__':
  main()
