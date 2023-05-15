# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities to read values from a bytearray."""

import struct


def _ParseULeb128(data, offset):
  """Returns a tuple of (uleb128 value, number of bytes occupied).

  From DWARF3 spec: http://dwarfstd.org/doc/Dwarf3.pdf

  Args:
    data: bytearray containing unsigned LEB128.
    offset: Location of the unsigned LEB128.
  """
  value = 0
  shift = 0
  cur_offset = offset
  while True:
    byte = data[cur_offset]
    cur_offset += 1
    value |= (byte & 0b01111111) << shift
    if (byte & 0b10000000) == 0:
      break
    shift += 7

  return value, cur_offset - offset


def _ParseSLeb128(data, offset):
  """Returns a tuple of (sleb128 value, number of bytes occupied).

  Args:
    data: bytearray containing signed LEB128.
    offset: Location of the signed LEB128.
  """
  value, size = _ParseULeb128(data, offset)
  sign_bit = 1 << min(31, size * 7 - 1)
  if (value & sign_bit) != 0:
    value -= sign_bit + sign_bit
  return value, size


class Mutf8DecodeError(Exception):
  def __init__(self, message, length, offset):
    message += ' (decoded string length: {}, string data offset: {:#x})'.format(
        length, offset)
    super().__init__(message)


class StreamReader:
  """Reads values from a bytearray using a seekable cursor.

  Integers are little endian.
  """

  def __init__(self, data):
    self._data = data
    self._pos = 0

  def Seek(self, offset):
    self._pos = offset

  def Tell(self):
    return self._pos

  def Skip(self, delta):
    self._pos += delta

  def NextStruct(self, fmt):
    ret = struct.unpack_from(fmt, self._data, self._pos)
    self._pos += struct.calcsize(fmt)
    return ret

  def NextBytes(self, n):
    old_pos = self._pos
    self._pos = min(len(self._data), old_pos + n)
    return self._data[old_pos:self._pos]

  def NextUByte(self):
    self._pos += 1
    return self._data[self._pos - 1]

  def NextUShort(self):
    self._pos += 2
    return struct.unpack_from('<H', self._data, self._pos - 2)[0]

  def NextUInt(self):
    self._pos += 4
    return struct.unpack_from('<I', self._data, self._pos - 4)[0]

  def NextULeb128(self):
    value, inc = _ParseULeb128(self._data, self._pos)
    self._pos += inc
    return value

  def NextSLeb128(self):
    value, inc = _ParseSLeb128(self._data, self._pos)
    self._pos += inc
    return value

  def NextMUtf8(self, string_length):
    """Returns the string located at the specified offset.

    See https://source.android.com/devices/tech/dalvik/dex-format#mutf-8

    Ported from the Android Java implementation:
    https://android.googlesource.com/platform/dalvik/+/fe107fb6e3f308ac5174ebdc5a794ee880c741d9/dx/src/com/android/dex/Mutf8.java#34

    Args:
      string_length: The length of the decoded string.
      offset: Offset to the beginning of the string.
    """
    offset = self._pos
    ret = ''

    for _ in range(string_length):
      a = self.NextUByte()
      if a == 0:
        raise Mutf8DecodeError('Early string termination encountered',
                               string_length, offset)
      if (a & 0x80) == 0x00:
        code = a
      elif (a & 0xe0) == 0xc0:
        b = self.NextUByte()
        if (b & 0xc0) != 0x80:
          raise Mutf8DecodeError('Error in byte 2', string_length, offset)
        code = ((a & 0x1f) << 6) | (b & 0x3f)
      elif (a & 0xf0) == 0xe0:
        b = self.NextUByte()
        c = self.NextUByte()
        if (b & 0xc0) != 0x80 or (c & 0xc0) != 0x80:
          raise Mutf8DecodeError('Error in byte 3 or 4', string_length, offset)
        code = ((a & 0x0f) << 12) | ((b & 0x3f) << 6) | (c & 0x3f)
      else:
        raise Mutf8DecodeError('Bad byte', string_length, offset)
      ret += chr(code)

    if self.NextUByte() != 0x00:
      raise Mutf8DecodeError('Expected string termination', string_length,
                             offset)

    return ret

  def NextString(self):
    string_length = self.NextULeb128()
    return self.NextMUtf8(string_length)

  def NextList(self, count, factory):
    return [factory(self) for _ in range(count)]

  def AlignUpTo(self, align_unit):
    off_by = self._pos % align_unit
    if off_by:
      self.Seek(self._pos + align_unit - off_by)
