# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Human Interface Device gadget module.

This gadget emulates a USB Human Interface Device. Multiple logical components
of a device can be composed together as separate "features" where each has its
own Report ID and will be called upon to answer get/set input/output/feature
report requests as necessary.
"""

from __future__ import print_function

import math
import struct
import uuid

import composite_gadget
import hid_constants
import usb_constants
import usb_descriptors


class HidCompositeFeature(composite_gadget.CompositeFeature):
  """Generic HID feature for a composite device.
  """

  def __init__(self, report_desc, features,
               packet_size=64, interval_ms=10, interface_number=0,
               interface_string=0,
               in_endpoint=0x81, out_endpoint=0x01):
    """Create a composite device feature implementing the HID protocol.

    Args:
      report_desc: HID report descriptor.
      features: Map between Report IDs and HidFeature objects to handle them.
      packet_size: Maximum interrupt packet size.
      interval_ms: Interrupt transfer interval in milliseconds.
      interface_number: Interface number for this feature (default 0).
      in_endpoint: Endpoint number for the IN endpoint (default 0x81).
      out_endpoint: Endpoint number for the OUT endpoint or None to disable
          the endpoint (default 0x01).

    Raises:
      ValueError: If any of the parameters are out of range.
    """
    fs_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=interface_number,
        bInterfaceClass=usb_constants.DeviceClass.HID,
        bInterfaceSubClass=0,  # Non-bootable.
        bInterfaceProtocol=0,  # None.
        iInterface=interface_string,
    )

    hs_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=interface_number,
        bInterfaceClass=usb_constants.DeviceClass.HID,
        bInterfaceSubClass=0,  # Non-bootable.
        bInterfaceProtocol=0,  # None.
        iInterface=interface_string,
    )

    hid_desc = usb_descriptors.HidDescriptor()
    hid_desc.AddDescriptor(hid_constants.DescriptorType.REPORT,
                           len(report_desc))
    fs_interface_desc.Add(hid_desc)
    hs_interface_desc.Add(hid_desc)

    fs_interval = math.ceil(math.log(interval_ms, 2)) + 1
    if fs_interval < 1 or fs_interval > 16:
      raise ValueError('Full speed interval out of range: {} ({} ms)'
                       .format(fs_interval, interval_ms))

    fs_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=in_endpoint,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=packet_size,
        bInterval=fs_interval
    ))

    hs_interval = math.ceil(math.log(interval_ms, 2)) + 4
    if hs_interval < 1 or hs_interval > 16:
      raise ValueError('High speed interval out of range: {} ({} ms)'
                       .format(hs_interval, interval_ms))

    hs_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=in_endpoint,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=packet_size,
        bInterval=hs_interval
    ))

    if out_endpoint is not None:
      fs_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=packet_size,
          bInterval=fs_interval
      ))
      hs_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=packet_size,
          bInterval=hs_interval
      ))

    super(HidCompositeFeature, self).__init__(
        [fs_interface_desc], [hs_interface_desc])
    self._report_desc = report_desc
    self._features = features
    self._interface_number = interface_number
    self._in_endpoint = in_endpoint
    self._out_endpoint = out_endpoint

  def Connected(self, gadget):
    super(HidCompositeFeature, self).Connected(gadget)
    for report_id, feature in self._features.iteritems():
      feature.Connected(self, report_id)

  def Disconnected(self):
    super(HidCompositeFeature, self).Disconnected()
    for feature in self._features.itervalues():
      feature.Disconnected()

  def StandardControlRead(self, recipient, request, value, index, length):
    if recipient == usb_constants.Recipient.INTERFACE:
      if index == self._interface_number:
        desc_type = value >> 8
        desc_index = value & 0xff
        if desc_type == hid_constants.DescriptorType.REPORT:
          if desc_index == 0:
            return self._report_desc[:length]

    return super(HidCompositeFeature, self).StandardControlRead(
        recipient, request, value, index, length)

  def ClassControlRead(self, recipient, request, value, index, length):
    """Handle class-specific control requests.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 7.2.

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
    if recipient != usb_constants.Recipient.INTERFACE:
      return None
    if index != self._interface_number:
      return None

    if request == hid_constants.Request.GET_REPORT:
      report_type, report_id = value >> 8, value & 0xFF
      print('GetReport(type={}, id={}, length={})'.format(
          report_type, report_id, length))
      return self.GetReport(report_type, report_id, length)

  def ClassControlWrite(self, recipient, request, value, index, data):
    """Handle class-specific control requests.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 7.2.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    if recipient != usb_constants.Recipient.INTERFACE:
      return None
    if index != self._interface_number:
      return None

    if request == hid_constants.Request.SET_REPORT:
      report_type, report_id = value >> 8, value & 0xFF
      print('SetReport(type={}, id={}, length={})'
            .format(report_type, report_id, len(data)))
      return self.SetReport(report_type, report_id, data)
    elif request == hid_constants.Request.SET_IDLE:
      duration, report_id = value >> 8, value & 0xFF
      print('SetIdle(duration={}, report={})'
            .format(duration, report_id))
      return True

  def GetReport(self, report_type, report_id, length):
    """Handle GET_REPORT requests.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 7.2.1.

    Args:
      report_type: Requested report type.
      report_id: Requested report ID.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    feature = self._features.get(report_id, None)
    if feature is None:
      return None

    if report_type == hid_constants.ReportType.INPUT:
      return feature.GetInputReport()[:length]
    elif report_type == hid_constants.ReportType.OUTPUT:
      return feature.GetOutputReport()[:length]
    elif report_type == hid_constants.ReportType.FEATURE:
      return feature.GetFeatureReport()[:length]

  def SetReport(self, report_type, report_id, data):
    """Handle SET_REPORT requests.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 7.2.2.

    Args:
      report_type: Report type.
      report_id: Report ID.
      data: Report data.

    Returns:
      True on success, None to stall the pipe.
    """
    feature = self._features.get(report_id, None)
    if feature is None:
      return None

    if report_type == hid_constants.ReportType.INPUT:
      return feature.SetInputReport(data)
    elif report_type == hid_constants.ReportType.OUTPUT:
      return feature.SetOutputReport(data)
    elif report_type == hid_constants.ReportType.FEATURE:
      return feature.SetFeatureReport(data)

  def SendReport(self, report_id, data):
    """Send a HID report.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 8.

    Args:
      report_id: Report ID associated with the data.
      data: Contents of the report.
    """
    if report_id == 0:
      self.SendPacket(self._in_endpoint, data)
    else:
      self.SendPacket(self._in_endpoint, struct.pack('B', report_id) + data)

  def ReceivePacket(self, endpoint, data):
    """Dispatch a report to the appropriate feature.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    section 8.

    Args:
      endpoint: Incoming endpoint (must be the Interrupt OUT pipe).
      data: Interrupt packet data.
    """
    assert endpoint == self._out_endpoint

    if 0 in self._features:
      self._features[0].SetOutputReport(data)
    elif len(data) >= 1:
      report_id, = struct.unpack('B', data[0])
      feature = self._features.get(report_id, None)
      if feature is None or feature.SetOutputReport(data[1:]) is None:
        self.HaltEndpoint(endpoint)


class HidFeature(object):
  """Represents a component of a HID gadget.

  A "feature" produces and consumes reports with a particular Report ID. For
  example a keyboard, mouse or vendor specific functionality.
  """

  def __init__(self):
    self._gadget = None
    self._report_id = None

  def Connected(self, my_gadget, report_id):
    self._gadget = my_gadget
    self._report_id = report_id

  def Disconnected(self):
    self._gadget = None
    self._report_id = None

  def IsConnected(self):
    return self._gadget is not None

  def SendReport(self, data):
    """Send a report with this feature's Report ID.

    Args:
      data: Report to send. If necessary the Report ID will be added.

    Raises:
      RuntimeError: If a report cannot be sent at this time.
    """
    if not self.IsConnected():
      raise RuntimeError('Device is not connected.')
    self._gadget.SendReport(self._report_id, data)

  def SetInputReport(self, data):
    """Handle an input report sent from the host.

    This function is called when a SET_REPORT(input) command for this class's
    Report ID is received. It should be overridden by a subclass.

    Args:
      data: Contents of the input report.
    """
    pass  # pragma: no cover

  def SetOutputReport(self, data):
    """Handle an feature report sent from the host.

    This function is called when a SET_REPORT(output) command or interrupt OUT
    transfer is received with this class's Report ID. It should be overridden
    by a subclass.

    Args:
      data: Contents of the output report.
    """
    pass  # pragma: no cover

  def SetFeatureReport(self, data):
    """Handle an feature report sent from the host.

    This function is called when a SET_REPORT(feature) command for this class's
    Report ID is received. It should be overridden by a subclass.

    Args:
      data: Contents of the feature report.
    """
    pass  # pragma: no cover

  def GetInputReport(self):
    """Handle a input report request from the host.

    This function is called when a GET_REPORT(input) command for this class's
    Report ID is received. It should be overridden by a subclass.

    Returns:
      The input report or None to stall the pipe.
    """
    pass  # pragma: no cover

  def GetOutputReport(self):
    """Handle a output report request from the host.

    This function is called when a GET_REPORT(output) command for this class's
    Report ID is received. It should be overridden by a subclass.

    Returns:
      The output report or None to stall the pipe.
    """
    pass  # pragma: no cover

  def GetFeatureReport(self):
    """Handle a feature report request from the host.

    This function is called when a GET_REPORT(feature) command for this class's
    Report ID is received. It should be overridden by a subclass.

    Returns:
      The feature report or None to stall the pipe.
    """
    pass  # pragma: no cover

class HidGadget(composite_gadget.CompositeGadget):
  """Generic HID gadget.
  """

  def __init__(self, report_desc, features, vendor_id, product_id,
               packet_size=64, interval_ms=10, out_endpoint=True,
               device_version=0x0100):
    """Create a HID gadget.

    Args:
      report_desc: HID report descriptor.
      features: Map between Report IDs and HidFeature objects to handle them.
      vendor_id: Device Vendor ID.
      product_id: Device Product ID.
      packet_size: Maximum interrupt packet size.
      interval_ms: Interrupt transfer interval in milliseconds.
      out_endpoint: Should this device have an interrupt OUT endpoint?
      device_version: Device version number.

    Raises:
      ValueError: If any of the parameters are out of range.
    """
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=vendor_id,
        idProduct=product_id,
        bcdUSB=0x0200,
        iManufacturer=1,
        iProduct=2,
        iSerialNumber=3,
        bcdDevice=device_version)

    if out_endpoint:
      out_endpoint = 0x01
    else:
      out_endpoint = None

    self._hid_feature = HidCompositeFeature(
        report_desc=report_desc,
        features=features,
        packet_size=packet_size,
        interval_ms=interval_ms,
        out_endpoint=out_endpoint)

    super(HidGadget, self).__init__(device_desc, [self._hid_feature])
    self.AddStringDescriptor(3, '{:06X}'.format(uuid.getnode()))

  def SendReport(self, report_id, data):
    self._hid_feature.SendReport(report_id, data)
