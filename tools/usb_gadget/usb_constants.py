# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""USB constant definitions.
"""


class DescriptorType(object):
  """Descriptor Types.

  See Universal Serial Bus Specification Revision 2.0 Table 9-5.
  """
  DEVICE = 1
  CONFIGURATION = 2
  STRING = 3
  INTERFACE = 4
  ENDPOINT = 5
  QUALIFIER = 6
  OTHER_SPEED_CONFIGURATION = 7
  BOS = 15
  DEVICE_CAPABILITY = 16


class CapabilityType(object):
  """Device capability types.

  See Universal Serial Bus 3.1 Specification, Revision 1.0 Table 9-14.
  """
  WIRELESS_USB = 0x01
  USB_20_EXTENSION = 0x02
  SUPERSPEED_USB = 0x03
  CONTAINER_ID = 0x04
  PLATFORM = 0x05
  POWER_DELIVERY_CAPABILITY = 0x06
  BATTERY_INFO_CAPABILITY = 0x07
  PD_CONSUMER_PORT_CAPABILITY = 0x08
  PD_PROVIDER_PORT_CAPABILITY = 0x09
  SUPERSPEED_PLUS = 0x0A
  PRECISION_TIME_MEASUREMENT = 0x0B
  WIRELESS_USB_EXT = 0x0C


class DeviceClass(object):
  """Class code.

  See http://www.usb.org/developers/defined_class.
  """
  PER_INTERFACE = 0
  AUDIO = 1
  COMM = 2
  HID = 3
  PHYSICAL = 5
  STILL_IMAGE = 6
  PRINTER = 7
  MASS_STORAGE = 8
  HUB = 9
  CDC_DATA = 10
  CSCID = 11
  CONTENT_SEC = 13
  VIDEO = 14
  VENDOR = 0xFF


class DeviceSubClass(object):
  """Subclass code.

  See http://www.usb.org/developers/defined_class.
  """
  PER_INTERFACE = 0
  VENDOR = 0xFF


class DeviceProtocol(object):
  """Protocol code.

  See http://www.usb.org/developers/defined_class.
  """
  PER_INTERFACE = 0
  VENDOR = 0xFF


class InterfaceClass(object):
  """Class code.

  See http://www.usb.org/developers/defined_class.
  """
  VENDOR = 0xFF


class InterfaceSubClass(object):
  """Subclass code.

  See http://www.usb.org/developers/defined_class.
  """
  VENDOR = 0xFF


class InterfaceProtocol(object):
  """Protocol code.

  See http://www.usb.org/developers/defined_class.
  """
  VENDOR = 0xFF


class TransferType(object):
  """Transfer Type.

  See http://www.usb.org/developers/defined_class.
  """
  MASK = 3
  CONTROL = 0
  ISOCHRONOUS = 1
  BULK = 2
  INTERRUPT = 3


class Dir(object):
  """Data transfer direction.

  See Universal Serial Bus Specification Revision 2.0 Table 9-2.
  """
  OUT = 0
  IN = 0x80


class Type(object):
  """Request Type.

  See Universal Serial Bus Specification Revision 2.0 Table 9-2.
  """
  MASK = 0x60
  STANDARD = 0x00
  CLASS = 0x20
  VENDOR = 0x40
  RESERVED = 0x60


class Recipient(object):
  """Request Recipient.

  See Universal Serial Bus Specification Revision 2.0 Table 9-2.
  """
  MASK = 0x1f
  DEVICE = 0
  INTERFACE = 1
  ENDPOINT = 2
  OTHER = 3


class Request(object):
  """Standard Request Codes.

  See Universal Serial Bus Specification Revision 2.0 Table 9-4.
  """
  GET_STATUS = 0x00
  CLEAR_FEATURE = 0x01
  SET_FEATURE = 0x03
  SET_ADDRESS = 0x05
  GET_DESCRIPTOR = 0x06
  SET_DESCRIPTOR = 0x07
  GET_CONFIGURATION = 0x08
  SET_CONFIGURATION = 0x09
  GET_INTERFACE = 0x0A
  SET_INTERFACE = 0x0B
  SYNCH_FRAME = 0x0C
  SET_SEL = 0x30
  SET_ISOCH_DELAY = 0x31


class Speed(object):
  UNKNOWN = 0
  LOW = 1
  FULL = 2
  HIGH = 3
  WIRELESS = 4
  SUPER = 5


class VendorID(object):
  GOOGLE = 0x18D1


class ProductID(object):
  GOOGLE_TEST_GADGET = 0x58F0
  GOOGLE_KEYBOARD_GADGET = 0x58F1
  GOOGLE_MOUSE_GADGET = 0x58F2
  GOOGLE_HID_ECHO_GADGET = 0x58F3
  GOOGLE_ECHO_GADGET = 0x58F4
  GOOGLE_COMPOSITE_ECHO_GADGET = 0x58F5
