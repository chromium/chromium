# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A composite USB gadget is built from multiple USB features.
"""

import gadget
import usb_constants
import usb_descriptors


class CompositeGadget(gadget.Gadget):
  """Basic functionality for a composite USB device.

  Composes multiple USB features into a single device.
  """

  def __init__(self, device_desc, features):
    """Create a USB gadget device.

    Args:
      device_desc: USB device descriptor.
      features: USB device features.
    """
    # dicts mapping interface numbers to features for FS and HS configurations
    self._fs_interface_feature_map = {}
    self._hs_interface_feature_map = {}

    fs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)
    hs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)
    for feature in features:
      for fs_interface in feature.GetFullSpeedInterfaces():
        fs_config_desc.AddInterface(fs_interface)
        self._fs_interface_feature_map[fs_interface.bInterfaceNumber] = feature
      for hs_interface in feature.GetHighSpeedInterfaces():
        hs_config_desc.AddInterface(hs_interface)
        self._hs_interface_feature_map[hs_interface.bInterfaceNumber] = feature

    super(CompositeGadget, self).__init__(
        device_desc, fs_config_desc, hs_config_desc)
    self._features = features

  def Connected(self, chip, speed):
    super(CompositeGadget, self).Connected(chip, speed)
    for feature in self._features:
      feature.Connected(self)

  def Disconnected(self):
    super(CompositeGadget, self).Disconnected()
    for feature in self._features:
      feature.Disconnected()

  def _GetInterfaceFeatureMap(self):
    if self.GetSpeed() == usb_constants.Speed.FULL:
      return self._fs_interface_feature_map
    elif self.GetSpeed() == usb_constants.Speed.HIGH:
      return self._hs_interface_feature_map
    else:
      raise RuntimeError('Device is not connected.')

  def ReceivePacket(self, endpoint, data):
    interface = self.GetInterfaceForEndpoint(endpoint)
    feature = self._GetInterfaceFeatureMap()[interface]
    feature.ReceivePacket(endpoint, data)

  def _GetFeatureForIndex(self, recipient, index):
    interface = None
    if recipient == usb_constants.Recipient.INTERFACE:
      interface = index
    elif recipient == usb_constants.Recipient.ENDPOINT:
      interface = self.GetInterfaceForEndpoint(index)

    if interface is not None:
      return self._GetInterfaceFeatureMap().get(interface)
    return None

  def StandardControlRead(self, recipient, request, value, index, length):
    response = super(CompositeGadget, self).StandardControlRead(
        recipient, request, value, index, length)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.StandardControlRead(
          recipient, request, value, index, length)

  def StandardControlWrite(self, recipient, request, value, index, data):
    response = super(CompositeGadget, self).StandardControlWrite(
        recipient, request, value, index, data)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.StandardControlWrite(
          recipient, request, value, index, data)

  def ClassControlRead(self, recipient, request, value, index, length):
    response = super(CompositeGadget, self).ClassControlRead(
        recipient, request, value, index, length)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.ClassControlRead(recipient, request, value, index, length)

  def ClassControlWrite(self, recipient, request, value, index, data):
    response = super(CompositeGadget, self).ClassControlWrite(
        recipient, request, value, index, data)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.ClassControlWrite(recipient, request, value, index, data)

  def VendorControlRead(self, recipient, request, value, index, length):
    response = super(CompositeGadget, self).VendorControlRead(
        recipient, request, value, index, length)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.VendorControlRead(recipient, request, value, index, length)

  def VendorControlWrite(self, recipient, request, value, index, data):
    response = super(CompositeGadget, self).VendorControlWrite(
        recipient, request, value, index, data)
    if response is not None:
      return response

    feature = self._GetFeatureForIndex(recipient, index)
    if feature:
      return feature.VendorControlWrite(recipient, request, value, index, data)


class CompositeFeature(object):
  def __init__(self, fs_interface_descs, hs_interface_descs):
    self._gadget = None
    self._fs_interface_descs = fs_interface_descs
    self._hs_interface_descs = hs_interface_descs

  def GetFullSpeedInterfaces(self):
    return self._fs_interface_descs

  def GetHighSpeedInterfaces(self):
    return self._hs_interface_descs

  def Connected(self, my_gadget):
    self._gadget = my_gadget

  def Disconnected(self):
    self._gadget = None

  def IsConnected(self):
    return self._gadget is not None

  def SendPacket(self, endpoint, data):
    if self._gadget is None:
      raise RuntimeError('Device is not connected.')
    self._gadget.SendPacket(endpoint, data)

  def HaltEndpoint(self, endpoint):
    if self._gadget is None:
      raise RuntimeError('Device is not connected.')
    self._gadget.HaltEndpoint(endpoint)

  def GetDescriptor(self, recipient, typ, index, lang, length):
    _ = recipient, typ, index, lang, length
    return None

  def StandardControlRead(self, recipient, request, value, index, length):
    """Handle standard USB control transfers.

    Args:
      recipient: Request recipient (interface or endpoint)
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

  def ClassControlRead(self, recipient, request, value, index, length):
    """Handle class-specific control transfers.

    Args:
      recipient: Request recipient (interface or endpoint)
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

    Args:
      recipient: Request recipient (interface or endpoint)
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

  def StandardControlWrite(self, recipient, request, value, index, data):
    """Handle standard USB control transfers.

    Args:
      recipient: Request recipient (interface or endpoint)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None

  def ClassControlWrite(self, recipient, request, value, index, data):
    """Handle class-specific control transfers.

    Args:
      recipient: Request recipient (interface or endpoint)
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

    Args:
      recipient: Request recipient (interface or endpoint)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None
