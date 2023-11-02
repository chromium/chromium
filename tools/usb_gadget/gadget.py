# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic USB gadget functionality.
"""

from __future__ import print_function

import struct

import msos20_descriptors
import usb_constants
import usb_descriptors


class Gadget(object):
  """Basic functionality for a USB device.

  Implements standard control requests assuming that a subclass will handle
  class- or vendor-specific requests.
  """

  def __init__(self, device_desc, fs_config_desc, hs_config_desc):
    """Create a USB gadget device.

    Args:
      device_desc: USB device descriptor.
      fs_config_desc: Low/full-speed device descriptor.
      hs_config_desc: High-speed device descriptor.
    """
    self._speed = usb_constants.Speed.UNKNOWN
    self._chip = None
    self._device_desc = device_desc
    self._fs_config_desc = fs_config_desc
    self._hs_config_desc = hs_config_desc
    # dict mapping language codes to a dict mapping indexes to strings
    self._strings = {}
    self._bos_descriptor = None
    # dict mapping interface numbers to a set of endpoint addresses
    self._active_endpoints = {}
    # dict mapping endpoint addresses to interfaces
    self._endpoint_interface_map = {}
    self._ms_vendor_code_v1 = None
    self._ms_vendor_code_v2 = None
    self._ms_compat_ids = {}
    self._ms_os20_config_subset = None

  def GetDeviceDescriptor(self):
    return self._device_desc

  def GetFullSpeedConfigurationDescriptor(self):
    return self._fs_config_desc

  def GetHighSpeedConfigurationDescriptor(self):
    return self._hs_config_desc

  def GetConfigurationDescriptor(self):
    if self._speed == usb_constants.Speed.FULL:
      return self._fs_config_desc
    elif self._speed == usb_constants.Speed.HIGH:
      return self._hs_config_desc
    else:
      raise RuntimeError('Device is not connected.')

  def GetSpeed(self):
    return self._speed

  def AddStringDescriptor(self, index, value, lang=0x0409):
    """Add a string descriptor to this device.

    Args:
      index: String descriptor index (matches 'i' fields in descriptors).
      value: The string.
      lang: Language code (default: English).

    Raises:
      ValueError: The index or language code is invalid.
    """
    if index < 1 or index > 255:
      raise ValueError('String descriptor index out of range.')
    if lang < 0 or lang > 0xffff:
      raise ValueError('String descriptor language code out of range.')

    lang_strings = self._strings.setdefault(lang, {})
    lang_strings[index] = value

  def EnableMicrosoftOSDescriptorsV1(self, vendor_code=0x01):
    if vendor_code < 0 or vendor_code > 255:
      raise ValueError('Vendor code out of range.')
    if vendor_code == self._ms_vendor_code_v1:
      raise ValueError('OS Descriptor v1 vendor code conflicts with v2.')

    self._ms_vendor_code_v1 = vendor_code

  def EnableMicrosoftOSDescriptorsV2(self, vendor_code=0x02):
    if vendor_code < 0 or vendor_code > 255:
      raise ValueError('Vendor code out of range.')
    if vendor_code == self._ms_vendor_code_v1:
      raise ValueError('OS Descriptor v2 vendor code conflicts with v1.')

    self._ms_vendor_code_v2 = vendor_code
    self._ms_os20_descriptor_set = \
        msos20_descriptors.DescriptorSetHeader(dwWindowsVersion=0x06030000)
    # Gadget devices currently only support one configuration. Contrary to
    # Microsoft's documentation the bConfigurationValue field should be set to
    # the index passed to GET_DESCRIPTOR that returned the configuration instead
    # of the configuration's bConfigurationValue field. (i.e. 0 instead of 1).
    #
    # https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/ae64282c-3bc3-49af-8391-4d174479d9e7/microsoft-os-20-descriptors-not-working-on-an-interface-of-a-composite-usb-device
    self._ms_os20_config_subset = msos20_descriptors.ConfigurationSubsetHeader(
        bConfigurationValue=0)
    self._ms_os20_descriptor_set.Add(self._ms_os20_config_subset)
    self._ms_os20_platform_descriptor = \
        msos20_descriptors.PlatformCapabilityDescriptor(
            dwWindowsVersion=0x06030000,
            bMS_VendorCode=self._ms_vendor_code_v2)
    self._ms_os20_platform_descriptor.SetDescriptorSet(
        self._ms_os20_descriptor_set)
    self.AddDeviceCapabilityDescriptor(self._ms_os20_platform_descriptor)

  def SetMicrosoftCompatId(self, interface_number, compat_id, sub_compat_id=''):
    self._ms_compat_ids[interface_number] = (compat_id, sub_compat_id)
    if self._ms_os20_config_subset is not None:
      function_header = msos20_descriptors.FunctionSubsetHeader(
          bFirstInterface=interface_number)
      function_header.Add(msos20_descriptors.CompatibleId(
          CompatibleID=compat_id, SubCompatibleID=sub_compat_id))
      self._ms_os20_config_subset.Add(function_header)

  def AddDeviceCapabilityDescriptor(self, device_capability):
    """Add a device capability descriptor to this device.

    Args:
      device_capability: The Descriptor object.
    """
    if self._bos_descriptor is None:
      self._bos_descriptor = usb_descriptors.BosDescriptor()
    self._bos_descriptor.AddDeviceCapability(device_capability)

  def Connected(self, chip, speed):
    """The device has been connected to a USB host.

    Args:
      chip: USB controller.
      speed: Connection speed.
    """
    self._speed = speed
    self._chip = chip

  def Disconnected(self):
    """The device has been disconnected from the USB host."""
    self._speed = usb_constants.Speed.UNKNOWN
    self._chip = None
    self._active_endpoints.clear()
    self._endpoint_interface_map.clear()

  def IsConnected(self):
    return self._chip is not None

  def ControlRead(self, request_type, request, value, index, length):
    """Handle a read on the control pipe (endpoint zero).

    Args:
      request_type: bmRequestType field of the setup packet.
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    assert request_type & usb_constants.Dir.IN
    typ = request_type & usb_constants.Type.MASK
    recipient = request_type & usb_constants.Recipient.MASK
    if typ == usb_constants.Type.STANDARD:
      return self.StandardControlRead(
          recipient, request, value, index, length)
    elif typ == usb_constants.Type.CLASS:
      return self.ClassControlRead(
          recipient, request, value, index, length)
    elif typ == usb_constants.Type.VENDOR:
      return self.VendorControlRead(
          recipient, request, value, index, length)

  def ControlWrite(self, request_type, request, value, index, data):
    """Handle a write to the control pipe (endpoint zero).

    Args:
      request_type: bmRequestType field of the setup packet.
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    assert not request_type & usb_constants.Dir.IN
    typ = request_type & usb_constants.Type.MASK
    recipient = request_type & usb_constants.Recipient.MASK
    if typ == usb_constants.Type.STANDARD:
      return self.StandardControlWrite(
          recipient, request, value, index, data)
    elif typ == usb_constants.Type.CLASS:
      return self.ClassControlWrite(
          recipient, request, value, index, data)
    elif typ == usb_constants.Type.VENDOR:
      return self.VendorControlWrite(
          recipient, request, value, index, data)

  def SendPacket(self, endpoint, data):
    """Send a data packet on the given endpoint.

    Args:
      endpoint: Endpoint address.
      data: Data buffer.

    Raises:
      ValueError: If the endpoint address is not valid.
      RuntimeError: If the device is not connected.
    """
    if self._chip is None:
      raise RuntimeError('Device is not connected.')
    if not endpoint & usb_constants.Dir.IN:
      raise ValueError('Cannot write to non-input endpoint.')
    self._chip.SendPacket(endpoint, data)

  def ReceivePacket(self, endpoint, data):
    """Handle an incoming data packet on one of the device's active endpoints.

    This method should be overridden by a subclass implementing endpoint-based
    data transfers.

    Args:
      endpoint: Endpoint address.
      data: Data buffer.
    """
    pass

  def HaltEndpoint(self, endpoint):
    """Signals a STALL condition to the host on the given endpoint.

    Args:
      endpoint: Endpoint address.
    """
    self._chip.HaltEndpoint(endpoint)

  def StandardControlRead(self, recipient, request, value, index, length):
    """Handle standard control transfers.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    if recipient == usb_constants.Recipient.DEVICE:
      if request == usb_constants.Request.GET_DESCRIPTOR:
        desc_type = value >> 8
        desc_index = value & 0xff
        desc_lang = index

        print('GetDescriptor(recipient={}, type={}, index={}, lang={})'.format(
            recipient, desc_type, desc_index, desc_lang))

        return self.GetDescriptor(recipient, desc_type, desc_index, desc_lang,
                                  length)

  def GetDescriptor(self, recipient, typ, index, lang, length):
    """Handle a standard GET_DESCRIPTOR request.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.3.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      typ: Descriptor type.
      index: Descriptor index.
      lang: Descriptor language code.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The value of the descriptor or None to stall the pipe.
    """
    if typ == usb_constants.DescriptorType.STRING:
      return self.GetStringDescriptor(index, lang, length)
    elif typ == usb_constants.DescriptorType.BOS:
      return self.GetBosDescriptor(length)

  def ClassControlRead(self, recipient, request, value, index, length):
    """Handle class-specific control transfers.

    This function should be overridden by a subclass implementing a particular
    device class.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    _ = recipient, request, value, index, length
    return None

  def VendorControlRead(self, recipient, request, value, index, length):
    """Handle vendor-specific control transfers.

    This function should be overridden by a subclass if implementing a device
    that responds to vendor-specific requests.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    if (self._ms_vendor_code_v1 is not None and
        request == self._ms_vendor_code_v1 and
        (recipient == usb_constants.Recipient.DEVICE or
         recipient == usb_constants.Recipient.INTERFACE)):
      return self.GetMicrosoftOSDescriptorV1(recipient, value, index, length)
    if (self._ms_vendor_code_v2 is not None and
        request == self._ms_vendor_code_v2 and
        recipient == usb_constants.Recipient.DEVICE and
        value == 0x0000 and
        index == 0x0007):
      return self.GetMicrosoftOSDescriptorV2(length)

    return None

  def StandardControlWrite(self, recipient, request, value, index, data):
    """Handle standard control transfers.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = data

    if request == usb_constants.Request.SET_CONFIGURATION:
      if recipient == usb_constants.Recipient.DEVICE:
        return self.SetConfiguration(value)
    elif request == usb_constants.Request.SET_INTERFACE:
      if recipient == usb_constants.Recipient.INTERFACE:
        return self.SetInterface(index, value)

  def ClassControlWrite(self, recipient, request, value, index, data):
    """Handle class-specific control transfers.

    This function should be overridden by a subclass implementing a particular
    device class.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None

  def VendorControlWrite(self, recipient, request, value, index, data):
    """Handle vendor-specific control transfers.

    This function should be overridden by a subclass if implementing a device
    that responds to vendor-specific requests.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None

  def GetStringDescriptor(self, index, lang, length):
    """Handle a GET_DESCRIPTOR(String) request from the host.

    Descriptor index 0 returns the set of languages supported by the device.
    All other indices return the string descriptors registered with those
    indices.

    See Universal Serial Bus Specification Revision 2.0 section 9.6.7.

    Args:
      index: Descriptor index.
      lang: Descriptor language code.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The string descriptor or None to stall the pipe if the descriptor is not
      found.
    """
    if index == 0:
      length = 2 + len(self._strings) * 2
      header = struct.pack('<BB', length, usb_constants.DescriptorType.STRING)
      lang_codes = [struct.pack('<H', lang)
                    for lang in self._strings.iterkeys()]
      buf = header + ''.join(lang_codes)
      assert len(buf) == length
      return buf[:length]
    if index == 0xEE and lang == 0 and self._ms_vendor_code_v1 is not None:
      # See https://msdn.microsoft.com/en-us/windows/hardware/gg463179 for the
      # definition of this special string descriptor.
      buf = (struct.pack('<BB', 18, usb_constants.DescriptorType.STRING) +
             'MSFT100'.encode('UTF-16LE') +
             struct.pack('<BB', self._ms_vendor_code_v1, 0))
      assert len(buf) == 18
      return buf[:length]
    elif lang not in self._strings:
      return None
    elif index not in self._strings[lang]:
      return None
    else:
      descriptor = usb_descriptors.StringDescriptor(
          bString=self._strings[lang][index])
      return descriptor.Encode()[:length]

  def GetMicrosoftOSDescriptorV1(self, recipient, value, index, length):
    """Handle a the Microsoft OS 1.0 Descriptor request from the host.

    See https://msdn.microsoft.com/en-us/windows/hardware/gg463179 for the
    format of these descriptors.

    Args:
      recipient: Request recipient (device or interface)
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The descriptor or None to stall the pipe if the descriptor is not
      supported.
    """
    _ = recipient, value
    if index == 0x0004:
      return self.GetMicrosoftCompatIds(length)

  def GetMicrosoftCompatIds(self, length):
    interfaces = self.GetConfigurationDescriptor().GetInterfaces()
    max_interface = max([iface.bInterfaceNumber for iface in interfaces])

    header = struct.pack('<IHHBxxxxxxx',
                         16 + 24 * (max_interface + 1),
                         0x0100,
                         0x0004,
                         max_interface + 1)
    if length <= len(header):
      return header[:length]

    buf = header
    for interface in xrange(max_interface + 1):
      compat_id, sub_compat_id = self._ms_compat_ids.get(interface, ('', ''))
      buf += struct.pack('<BB8s8sxxxxxx',
                         interface, 0x01, compat_id, sub_compat_id)
    return buf[:length]

  def GetMicrosoftOSDescriptorV2(self, length):
    return self._ms_os20_descriptor_set.Encode()[:length]

  def GetBosDescriptor(self, length):
    """Handle a GET_DESCRIPTOR(BOS) request from the host.

    Device capability descriptors can be added to the Binary Device Object Store
    returned by this method by calling AddDeviceCapabilityDescriptor.

    See Universal Serial Bus 3.1 Specification, Revision 1.0 section 9.6.2.

    Args:
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The device's binary object store descriptor or None to stall the pipe if
      no device capability descriptors have been configured.
    """
    if self._bos_descriptor is None:
      return None

    return self._bos_descriptor.Encode()[:length]

  def SetConfiguration(self, index):
    """Handle a SET_CONFIGURATION request from the host.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.7.

    Args:
      index: Configuration index selected.

    Returns:
      True on success, None on error to stall the pipe.
    """
    print('SetConfiguration({})'.format(index))

    for endpoint_addrs in self._active_endpoints.values():
      for endpoint_addr in endpoint_addrs:
        self._chip.StopEndpoint(endpoint_addr)
      endpoint_addrs.clear()
    self._endpoint_interface_map.clear();

    if index == 0:
      # SET_CONFIGRATION(0) puts the device into the Address state which
      # Windows does before suspending the port.
      return True
    elif index != 1:
      return None

    config_desc = self.GetConfigurationDescriptor()
    for interface_desc in config_desc.GetInterfaces():
      if interface_desc.bAlternateSetting != 0:
        continue
      endpoint_addrs = self._active_endpoints.setdefault(
          interface_desc.bInterfaceNumber, set())
      for endpoint_desc in interface_desc.GetEndpoints():
        self._chip.StartEndpoint(endpoint_desc)
        endpoint_addrs.add(endpoint_desc.bEndpointAddress)
        self._endpoint_interface_map[endpoint_desc.bEndpointAddress] = \
            interface_desc.bInterfaceNumber
    return True

  def SetInterface(self, interface, alt_setting):
    """Handle a SET_INTERFACE request from the host.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.10.

    Args:
      interface: Interface number to configure.
      alt_setting: Alternate setting to select.

    Returns:
      True on success, None on error to stall the pipe.
    """
    print('SetInterface({}, {})'.format(interface, alt_setting))

    config_desc = self.GetConfigurationDescriptor()
    interface_desc = None
    for interface_option in config_desc.GetInterfaces():
      if (interface_option.bInterfaceNumber == interface and
          interface_option.bAlternateSetting == alt_setting):
        interface_desc = interface_option
    if interface_desc is None:
      return None

    endpoint_addrs = self._active_endpoints.setdefault(interface, set())
    for endpoint_addr in endpoint_addrs:
      self._chip.StopEndpoint(endpoint_addr)
      del self._endpoint_interface_map[endpoint_addr]
    for endpoint_desc in interface_desc.GetEndpoints():
      self._chip.StartEndpoint(endpoint_desc)
      endpoint_addrs.add(endpoint_desc.bEndpointAddress)
      self._endpoint_interface_map[endpoint_desc.bEndpointAddress] = \
          interface_desc.bInterfaceNumber
    return True

  def GetInterfaceForEndpoint(self, endpoint_addr):
    return self._endpoint_interface_map.get(endpoint_addr)
