# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utility functions for constructing HID report descriptors.
"""

import struct

import hid_constants


def ReportDescriptor(*items):
  return ''.join(items)


def _PackItem(tag, typ, value=0, force_length=0):
  """Pack a multibyte value.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 5.8.

  Args:
    tag: Item tag.
    typ: Item type.
    value: Item value.
    force_length: Force packing to a specific width.

  Returns:
    Packed string.
  """
  if value == 0 and force_length <= 0:
    return struct.pack('<B', tag << 4 | typ << 2 | 0)
  elif value <= 0xff and force_length <= 1:
    return struct.pack('<BB', tag << 4 | typ << 2 | 1, value)
  elif value <= 0xffff and force_length <= 2:
    return struct.pack('<BH', tag << 4 | typ << 2 | 2, value)
  elif value <= 0xffffffff and force_length <= 4:
    return struct.pack('<BI', tag << 4 | typ << 2 | 3, value)
  else:
    raise NotImplementedError('Long items are not implemented.')


def _DefineItem(name, tag, typ):
  """Create a function which encodes a HID item.

  Args:
    name: Function name.
    tag: Item tag.
    typ: Item type.

  Returns:
    A function which encodes a HID item of the given type.
  """
  assert tag >= 0 and tag <= 0xF
  assert typ >= 0 and typ <= 3

  def EncodeItem(value=0, force_length=0):
    return _PackItem(tag, typ, value, force_length)

  EncodeItem.__name__ = name
  return EncodeItem


def _DefineMainItem(name, tag):
  """Create a function which encodes a HID Main item.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 6.2.2.4.

  Args:
    name: Function name.
    tag: Item tag.

  Returns:
    A function which encodes a HID item of the given type.

  Raises:
    ValueError: If the tag value is out of range.
  """
  assert tag >= 0 and tag <= 0xF

  def EncodeMainItem(*properties):
    value = 0
    for bit, is_set in properties:
      if is_set:
        value |= 1 << bit
    return _PackItem(tag, hid_constants.Scope.MAIN, value, force_length=1)

  EncodeMainItem.__name__ = name
  return EncodeMainItem

Input = _DefineMainItem('Input', 8)
Output = _DefineMainItem('Output', 9)
Feature = _DefineMainItem('Feature', 11)

# Input, Output and Feature Item Properties
#
# See Device Class Definition for Human Interface Devices (HID) Version 1.11
# section 6.2.2.5.
Data = (0, False)
Constant = (0, True)
Array = (1, False)
Variable = (1, True)
Absolute = (2, False)
Relative = (2, True)
NoWrap = (3, False)
Wrap = (3, True)
Linear = (4, False)
NonLinear = (4, True)
PreferredState = (5, False)
NoPreferred = (5, True)
NoNullPosition = (6, False)
NullState = (6, True)
NonVolatile = (7, False)
Volatile = (7, True)
BitField = (8, False)
BufferedBytes = (8, True)


def Collection(typ, *items):
  start = struct.pack('<BB', 0xA1, typ)
  end = struct.pack('<B', 0xC0)
  return start + ''.join(items) + end

# Global Items
#
# See Device Class Definition for Human Interface Devices (HID) Version 1.11
# section 6.2.2.7.
UsagePage = _DefineItem('UsagePage', 0, hid_constants.Scope.GLOBAL)
LogicalMinimum = _DefineItem('LogicalMinimum', 1, hid_constants.Scope.GLOBAL)
LogicalMaximum = _DefineItem('LogicalMaximum', 2, hid_constants.Scope.GLOBAL)
PhysicalMinimum = _DefineItem('PhysicalMinimum', 3, hid_constants.Scope.GLOBAL)
PhysicalMaximum = _DefineItem('PhysicalMaximum', 4, hid_constants.Scope.GLOBAL)
UnitExponent = _DefineItem('UnitExponent', 5, hid_constants.Scope.GLOBAL)
Unit = _DefineItem('Unit', 6, hid_constants.Scope.GLOBAL)
ReportSize = _DefineItem('ReportSize', 7, hid_constants.Scope.GLOBAL)
ReportID = _DefineItem('ReportID', 8, hid_constants.Scope.GLOBAL)
ReportCount = _DefineItem('ReportCount', 9, hid_constants.Scope.GLOBAL)
Push = _DefineItem('Push', 10, hid_constants.Scope.GLOBAL)
Pop = _DefineItem('Pop', 11, hid_constants.Scope.GLOBAL)

# Local Items
#
# See Device Class Definition for Human Interface Devices (HID) Version 1.11
# section 6.2.2.8.
Usage = _DefineItem('Usage', 0, hid_constants.Scope.LOCAL)
UsageMinimum = _DefineItem('UsageMinimum', 1, hid_constants.Scope.LOCAL)
UsageMaximum = _DefineItem('UsageMaximum', 2, hid_constants.Scope.LOCAL)
DesignatorIndex = _DefineItem('DesignatorIndex', 3, hid_constants.Scope.LOCAL)
DesignatorMinimum = _DefineItem('DesignatorMinimum', 4,
                                hid_constants.Scope.LOCAL)
DesignatorMaximum = _DefineItem('DesignatorMaximum', 5,
                                hid_constants.Scope.LOCAL)
StringIndex = _DefineItem('StringIndex', 7, hid_constants.Scope.LOCAL)
StringMinimum = _DefineItem('StringMinimum', 8, hid_constants.Scope.LOCAL)
StringMaximum = _DefineItem('StringMaximum', 9, hid_constants.Scope.LOCAL)
Delimiter = _DefineItem('Delimiter', 10, hid_constants.Scope.LOCAL)
