# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""HID constant definitions.
"""

import usb_constants


class DescriptorType(object):
  """Class descriptors.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 7.1.
  """
  HID = usb_constants.Type.CLASS | 0x01
  REPORT = usb_constants.Type.CLASS | 0x02
  PHYSICAL = usb_constants.Type.CLASS | 0x03


class Scope(object):
  """Item scope.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 6.2.2.2.
  """
  MAIN = 0
  GLOBAL = 1
  LOCAL = 2


class CollectionType(object):
  """Collection types.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 6.2.2.4.
  """
  PHYSICAL = 0
  APPLICATION = 1
  LOGICAL = 2
  REPORT = 3
  NAMED_ARRAY = 4
  USAGE_SWITCH = 5
  USAGE_MODIFIER = 6


class Request(object):
  """Class specific requests.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 7.2.
  """
  GET_REPORT = 1
  GET_IDLE = 2
  GET_PROTOCOL = 3
  SET_REPORT = 9
  SET_IDLE = 0x0A
  SET_PROTOCOL = 0x0B


class ReportType(object):
  """Report types.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 7.2.1.
  """
  INPUT = 1
  OUTPUT = 2
  FEATURE = 3


class ModifierKey(object):
  """Keyboard modifier key report values.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 8.3 and HID Usage Tables Version 1.1 Table 12.
  """
  L_CTRL = 0x01
  L_SHIFT = 0x02
  L_ALT = 0x04
  L_GUI = 0x08
  R_CTRL = 0x10
  R_SHIFT = 0x20
  R_ALT = 0x40
  R_GUI = 0x80


class LED(object):
  """Keyboard LED report values.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section B.1 and HID Usage Tables Version 1.1 Table 13.
  """
  NUM_LOCK = 0x01
  CAPS_LOCK = 0x02
  SCROLL_LOCK = 0x04
  COMPOSE = 0x08
  KANA = 0x10


class Mouse(object):
  """Mouse button report values.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section B.2.
  """
  BUTTON_1 = 0x01
  BUTTON_2 = 0x02
  BUTTON_3 = 0x04


KEY_CODES = {}
for key, code in zip(xrange(ord('a'), ord('z') + 1), xrange(4, 30)):
  KEY_CODES[chr(key)] = code
for key, code in zip(xrange(ord('1'), ord('9') + 1), xrange(30, 39)):
  KEY_CODES[chr(key)] = code
for key, code in zip(['Enter', 'Esc', 'Backspace', 'Tab', ' '], xrange(40, 45)):
  KEY_CODES[key] = code
for key, code in zip('-=[]\\', xrange(45, 50)):
  KEY_CODES[key] = code
for key, code in zip(';\'`,./', xrange(51, 57)):
  KEY_CODES[key] = code
for key, code in zip(
    ['CapsLock', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10',
     'F11', 'F12', 'PrintScreen', 'ScrollLock', 'Pause', 'Insert', 'Home',
     'PageUp', 'PageDown', 'Delete', 'End', 'PageDown', 'RightArrow',
     'LeftArrow', 'DownArrow', 'UpArrow', 'NumLock'],
    xrange(57, 84)):
  KEY_CODES[key] = code

SHIFT_KEY_CODES = {}
for key, code in zip(xrange(ord('A'), ord('Z') + 1), xrange(4, 30)):
  SHIFT_KEY_CODES[chr(key)] = code
for key, code in zip('!@#$%^&*()', xrange(30, 40)):
  SHIFT_KEY_CODES[key] = code
for key, code in zip('_+{}|', xrange(45, 50)):
  SHIFT_KEY_CODES[key] = code
for key, code in zip(':"~<>?', xrange(51, 57)):
  SHIFT_KEY_CODES[key] = code
