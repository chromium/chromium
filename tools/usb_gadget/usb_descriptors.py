# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""USB descriptor generation utilities.

Classes to represent and generate USB descriptors.
"""

import struct

import hid_constants
import usb_constants


class Field(object):
  """USB descriptor field information."""

  def __init__(self, name, str_fmt, struct_fmt, required):
    """Define a new USB descriptor field.

    Args:
      name: Name of the field.
      str_fmt: Python 'string' module format string for this field.
      struct_fmt: Python 'struct' module format string for this field.
      required: Is this a required field?
    """
    self.name = name
    self.str_fmt = str_fmt
    self.struct_fmt = struct_fmt
    self.required = required

  def Format(self, value):
    return self.str_fmt.format(value)


class Descriptor(object):
  """Base class for USB descriptor types.

  This class provides general functionality for creating object types that
  represent USB descriptors. The AddField and related methods are used to
  define the fields of each structure. Fields can then be set using keyword
  arguments to the object constructor or by accessing properties on the object.
  """

  _fields = None

  @classmethod
  def AddField(cls, name, struct_fmt, str_fmt='{}', default=None):
    """Adds a user-specified field to this descriptor.

    Adds a field to the binary structure representing this descriptor. The field
    can be set by passing a keyword argument name=... to the object constructor
    will be accessible as foo.name on any instance.

    If no default value is provided then the constructor will through an
    exception if this field is not one of the provided keyword arguments.

    Args:
      name: String name of the field.
      struct_fmt: Python 'struct' module format string for this field.
      str_fmt: Python 'string' module format string for this field.
      default: Default value.
    """
    if cls._fields is None:
      cls._fields = []
    cls._fields.append(Field(name, str_fmt, struct_fmt, default is None))

    member_name = '_{}'.format(name)
    def Setter(self, value):
      setattr(self, member_name, value)
    def Getter(self):
      try:
        return getattr(self, member_name)
      except AttributeError:
        assert default is not None
        return default

    setattr(cls, name, property(Getter, Setter))

  @classmethod
  def AddFixedField(cls, name, struct_fmt, value, str_fmt='{}'):
    """Adds a constant field to this descriptor.

    Adds a constant field to the binary structure representing this descriptor.
    The field will be accessible as foo.name on any instance.

    The value of this field may not be given as a constructor parameter or
    set on an existing instance.

    Args:
      name: String name of the field.
      struct_fmt: Python 'struct' module format string for this field.
      value: Field value.
      str_fmt: Python 'string' module format string for this field.
    """
    if cls._fields is None:
      cls._fields = []
    cls._fields.append(Field(name, str_fmt, struct_fmt, False))

    def Setter(unused_self, unused_value):
      raise RuntimeError('{} is a fixed field.'.format(name))
    def Getter(unused_self):
      return value

    setattr(cls, name, property(Getter, Setter))

  @classmethod
  def AddComputedField(cls, name, struct_fmt, property_name, str_fmt='{}'):
    """Adds a constant field to this descriptor.

    Adds a field to the binary structure representing this descriptor whos value
    is equal to an object property. The field will be accessible as foo.name on
    any instance.

    The value of this field may not be given as a constructor parameter or
    set on an existing instance.

    Args:
      name: String name of the field.
      struct_fmt: Python 'struct' module format string for this field.
      property_name: Property to read.
      str_fmt: Python 'string' module format string for this field.
    """
    if cls._fields is None:
      cls._fields = []
    cls._fields.append(Field(name, str_fmt, struct_fmt, False))

    def Setter(unused_self, unused_value):
      raise RuntimeError('{} is a computed field.'.format(name))
    def Getter(self):
      return getattr(self, property_name)

    setattr(cls, name, property(Getter, Setter))

  def __init__(self, **kwargs):
    """Constructs a new instance of this descriptor.

    All fields which do not have a default value and are not fixed or computed
    from a property must be specified as keyword arguments.

    Args:
      **kwargs: Field values.

    Raises:
      TypeError: A required field was missing or an unexpected field was given.
    """
    fields = {field.name for field in self._fields}
    required_fields = {field.name for field in self._fields if field.required}

    for arg, value in kwargs.iteritems():
      if arg not in fields:
        raise TypeError('Unexpected field: {}'.format(arg))

      setattr(self, arg, value)
      required_fields.discard(arg)

    if required_fields:
      raise TypeError('Missing fields: {}'.format(', '.join(required_fields)))

  @property
  def fmt(self):
    """Returns the Python 'struct' module format string for this descriptor."""
    return '<{}'.format(''.join([field.struct_fmt for field in self._fields]))

  @property
  def struct_size(self):
    """Returns the size of the struct defined by fmt."""
    return struct.calcsize(self.fmt)

  @property
  def total_size(self):
    """Returns the total size of this descriptor."""
    return self.struct_size

  def Encode(self):
    """Returns the binary representation of this descriptor."""
    values = [getattr(self, field.name) for field in self._fields]
    return struct.pack(self.fmt, *values)

  def __str__(self):
    max_length = max(len(field.name) for field in self._fields)

    return '{}:\n  {}'.format(
        self.__class__.__name__,
        '\n  '.join('{} {}'.format(
            '{}:'.format(field.name).ljust(max_length+1),
            field.Format(getattr(self, field.name))
        ) for field in self._fields)
    )


class DeviceDescriptor(Descriptor):
  """Standard Device Descriptor.

  See Universal Serial Bus Specification Revision 2.0 Table 9-8.
  """
  pass

DeviceDescriptor.AddComputedField('bLength', 'B', 'struct_size')
DeviceDescriptor.AddFixedField('bDescriptorType', 'B',
                               usb_constants.DescriptorType.DEVICE)
DeviceDescriptor.AddField('bcdUSB', 'H', default=0x0200, str_fmt='0x{:04X}')
DeviceDescriptor.AddField('bDeviceClass', 'B',
                          default=usb_constants.DeviceClass.PER_INTERFACE)
DeviceDescriptor.AddField('bDeviceSubClass', 'B',
                          default=usb_constants.DeviceSubClass.PER_INTERFACE)
DeviceDescriptor.AddField('bDeviceProtocol', 'B',
                          default=usb_constants.DeviceProtocol.PER_INTERFACE)
DeviceDescriptor.AddField('bMaxPacketSize0', 'B', default=64)
DeviceDescriptor.AddField('idVendor', 'H', str_fmt='0x{:04X}')
DeviceDescriptor.AddField('idProduct', 'H', str_fmt='0x{:04X}')
DeviceDescriptor.AddField('bcdDevice', 'H', str_fmt='0x{:04X}')
DeviceDescriptor.AddField('iManufacturer', 'B', default=0)
DeviceDescriptor.AddField('iProduct', 'B', default=0)
DeviceDescriptor.AddField('iSerialNumber', 'B', default=0)
DeviceDescriptor.AddField('bNumConfigurations', 'B', default=1)


class DescriptorContainer(Descriptor):
  """Super-class for descriptors which contain more descriptors.

  This class adds the ability for a descriptor to have an array of additional
  descriptors which follow it.
  """

  def __init__(self, **kwargs):
    super(DescriptorContainer, self).__init__(**kwargs)
    self._descriptors = []

  @property
  def total_size(self):
    return self.struct_size + sum([descriptor.total_size
                                   for descriptor in self._descriptors])

  def Add(self, descriptor):
    self._descriptors.append(descriptor)

  def Encode(self):
    bufs = [super(DescriptorContainer, self).Encode()]
    bufs.extend(descriptor.Encode() for descriptor in self._descriptors)
    return ''.join(bufs)

  def __str__(self):
    return '{}\n{}'.format(super(DescriptorContainer, self).__str__(),
                           '\n'.join(str(descriptor)
                                     for descriptor in self._descriptors))


class StringDescriptor(Descriptor):
  """Standard String Descriptor.

  See Universal Serial Bus Specification Revision 2.0 Table 9-16.
  """

  def __init__(self, **kwargs):
    self.bString = kwargs.pop('bString', '')
    super(StringDescriptor, self).__init__(**kwargs)

  @property
  def total_size(self):
    return self.struct_size + len(self.bString.encode('UTF-16LE'))

  def Encode(self):
    return (
        super(StringDescriptor, self).Encode() +
        self.bString.encode('UTF-16LE'))

  def __str__(self):
    return '{}\n  bString:         "{}"'.format(
        super(StringDescriptor, self).__str__(), self.bString)

StringDescriptor.AddComputedField('bLength', 'B', 'total_size')
StringDescriptor.AddFixedField(
    'bDescriptorType', 'B', usb_constants.DescriptorType.STRING)


class ConfigurationDescriptor(DescriptorContainer):
  """Standard Configuration Descriptor.

  See Universal Serial Bus Specification Revision 2.0 Table 9-10.
  """

  def __init__(self, **kwargs):
    super(ConfigurationDescriptor, self).__init__(**kwargs)
    self._interfaces = {}

  @property
  def num_interfaces(self):
    interface_numbers = {key[0] for key in self._interfaces.iterkeys()}
    return len(interface_numbers)

  def AddInterface(self, interface):
    key = (interface.bInterfaceNumber, interface.bAlternateSetting)
    if key in self._interfaces:
      raise RuntimeError('Interface {} (alternate {}) already defined.'
                         .format(key[0], key[1]))
    self._interfaces[key] = interface
    self.Add(interface)

  def GetInterfaces(self):
    return self._interfaces.values()

ConfigurationDescriptor.AddComputedField('bLength', 'B', 'struct_size')
ConfigurationDescriptor.AddFixedField(
    'bDescriptorType', 'B', usb_constants.DescriptorType.CONFIGURATION)
ConfigurationDescriptor.AddComputedField('wTotalLength', 'H', 'total_size')
ConfigurationDescriptor.AddComputedField('bNumInterfaces', 'B',
                                         'num_interfaces')
ConfigurationDescriptor.AddField('bConfigurationValue', 'B', default=1)
ConfigurationDescriptor.AddField('iConfiguration', 'B', default=0)
ConfigurationDescriptor.AddField('bmAttributes', 'B', str_fmt='0x{:02X}')
ConfigurationDescriptor.AddField('MaxPower', 'B')


class InterfaceDescriptor(DescriptorContainer):
  """Standard Interface Descriptor.

  See Universal Serial Bus Specification Revision 2.0 Table 9-12.
  """

  def __init__(self, **kwargs):
    super(InterfaceDescriptor, self).__init__(**kwargs)
    self._endpoints = {}

  @property
  def num_endpoints(self):
    return len(self._endpoints)

  def AddEndpoint(self, endpoint):
    if endpoint.bEndpointAddress in self._endpoints:
      raise RuntimeError('Endpoint 0x{:02X} already defined on this interface.'
                         .format(endpoint.bEndpointAddress))
    self._endpoints[endpoint.bEndpointAddress] = endpoint
    self.Add(endpoint)

  def GetEndpoints(self):
    return self._endpoints.values()

InterfaceDescriptor.AddComputedField('bLength', 'B', 'struct_size')
InterfaceDescriptor.AddFixedField('bDescriptorType', 'B',
                                  usb_constants.DescriptorType.INTERFACE)
InterfaceDescriptor.AddField('bInterfaceNumber', 'B')
InterfaceDescriptor.AddField('bAlternateSetting', 'B', default=0)
InterfaceDescriptor.AddComputedField('bNumEndpoints', 'B', 'num_endpoints')
InterfaceDescriptor.AddField('bInterfaceClass', 'B',
                             default=usb_constants.InterfaceClass.VENDOR)
InterfaceDescriptor.AddField('bInterfaceSubClass', 'B',
                             default=usb_constants.InterfaceSubClass.VENDOR)
InterfaceDescriptor.AddField('bInterfaceProtocol', 'B',
                             default=usb_constants.InterfaceProtocol.VENDOR)
InterfaceDescriptor.AddField('iInterface', 'B', default=0)


class EndpointDescriptor(Descriptor):
  """Standard Endpoint Descriptor.

  See Universal Serial Bus Specification Revision 2.0 Table 9-13.
  """
  pass

EndpointDescriptor.AddComputedField('bLength', 'B', 'struct_size')
EndpointDescriptor.AddFixedField('bDescriptorType', 'B',
                                 usb_constants.DescriptorType.ENDPOINT)
EndpointDescriptor.AddField('bEndpointAddress', 'B', str_fmt='0x{:02X}')
EndpointDescriptor.AddField('bmAttributes', 'B', str_fmt='0x{:02X}')
EndpointDescriptor.AddField('wMaxPacketSize', 'H')
EndpointDescriptor.AddField('bInterval', 'B')


class HidDescriptor(Descriptor):
  """HID Descriptor.

  See Device Class Definition for Human Interface Devices (HID) Version 1.11
  section 6.2.1.
  """

  def __init__(self, **kwargs):
    super(HidDescriptor, self).__init__(**kwargs)
    self._descriptors = []

  def AddDescriptor(self, typ, length):
    self._descriptors.append((typ, length))

  @property
  def struct_size(self):
    return super(HidDescriptor, self).struct_size + self.num_descriptors * 3

  @property
  def num_descriptors(self):
    return len(self._descriptors)

  def Encode(self):
    bufs = [super(HidDescriptor, self).Encode()]
    bufs.extend(struct.pack('<BH', typ, length)
                for typ, length in self._descriptors)
    return ''.join(bufs)

  def __str__(self):
    return '{}\n{}'.format(
        super(HidDescriptor, self).__str__(),
        '\n'.join('  bDescriptorType: 0x{:02X}\n  wDescriptorLength: {}'
                  .format(typ, length) for typ, length in self._descriptors))

HidDescriptor.AddComputedField('bLength', 'B', 'struct_size')
HidDescriptor.AddFixedField('bDescriptorType', 'B',
                            hid_constants.DescriptorType.HID)
HidDescriptor.AddField('bcdHID', 'H', default=0x0111, str_fmt='0x{:04X}')
HidDescriptor.AddField('bCountryCode', 'B', default=0)
HidDescriptor.AddComputedField('bNumDescriptors', 'B', 'num_descriptors')


class BosDescriptor(DescriptorContainer):
  """Binary Device Object Store descriptor.

  See Universal Serial Bus 3.1 Specification, Revision 1.0 Table 9-12.
  """

  def __init__(self, **kwargs):
    super(BosDescriptor, self).__init__(**kwargs)
    self._device_caps = []

  @property
  def num_device_caps(self):
    return len(self._device_caps)

  def AddDeviceCapability(self, device_capability):
    self._device_caps.append(device_capability)
    self.Add(device_capability)

  def GetDeviceCapabilities(self):
    return self._device_caps

BosDescriptor.AddComputedField('bLength', 'B', 'struct_size')
BosDescriptor.AddFixedField('bDescriptorType', 'B',
                            usb_constants.DescriptorType.BOS)
BosDescriptor.AddComputedField('wTotalLength', 'H', 'total_size')
BosDescriptor.AddComputedField('bNumDeviceCaps', 'B', 'num_device_caps')


class ContainerIdDescriptor(Descriptor):
  """Container ID descriptor.

  See Universal Serial Bus 3.1 Specification, Revision 1.0 Table 9-17.
  """
  pass

ContainerIdDescriptor.AddComputedField('bLength', 'B', 'struct_size')
ContainerIdDescriptor.AddFixedField(
    'bDescriptorType', 'B', usb_constants.DescriptorType.DEVICE_CAPABILITY)
ContainerIdDescriptor.AddFixedField(
    'bDevCapabilityType', 'B', usb_constants.CapabilityType.CONTAINER_ID)
ContainerIdDescriptor.AddFixedField('bReserved', 'B', 0)
ContainerIdDescriptor.AddField('ContainerID', '16s')
