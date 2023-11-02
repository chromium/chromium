# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A HID-class echo device.

This module provides a HID feature and HID device that can be used as an
echo test for HID drivers. The device exposes vendor-specific input, output
and feature usages that transmit 8 bytes of data. Data written sent as an
output report is echoed as an input report. The value of the feature report
can be written and read with control transfers.
"""

import struct

import hid_constants
import hid_descriptors
import hid_gadget
import usb_constants


class EchoFeature(hid_gadget.HidFeature):

  REPORT_DESC = hid_descriptors.ReportDescriptor(
      hid_descriptors.UsagePage(0xFF00),  # Vendor Defined
      hid_descriptors.Usage(0),
      hid_descriptors.Collection(
          hid_constants.CollectionType.APPLICATION,
          hid_descriptors.LogicalMinimum(0, force_length=1),
          hid_descriptors.LogicalMaximum(255, force_length=2),
          hid_descriptors.ReportSize(8),
          hid_descriptors.ReportCount(8),
          hid_descriptors.Usage(0),
          hid_descriptors.Input(hid_descriptors.Data,
                                hid_descriptors.Variable,
                                hid_descriptors.Absolute),
          hid_descriptors.Usage(0),
          hid_descriptors.Output(hid_descriptors.Data,
                                 hid_descriptors.Variable,
                                 hid_descriptors.Absolute),
          hid_descriptors.Usage(0),
          hid_descriptors.Feature(hid_descriptors.Data,
                                  hid_descriptors.Variable,
                                  hid_descriptors.Absolute)
      )
  )

  def __init__(self):
    super(EchoFeature, self).__init__()
    self._input_output_report = 0
    self._feature_report = 0

  def SetInputReport(self, data):
    self._input_output_report, = struct.unpack('<Q', data)
    self.SendReport(struct.pack('<Q', self._input_output_report))
    return True

  def SetOutputReport(self, data):
    self._input_output_report, = struct.unpack('<Q', data)
    self.SendReport(struct.pack('<Q', self._input_output_report))
    return True

  def SetFeatureReport(self, data):
    self._feature_report, = struct.unpack('<Q', data)
    return True

  def GetInputReport(self):
    return struct.pack('<Q', self._input_output_report)

  def GetOutputReport(self):
    return struct.pack('<Q', self._input_output_report)

  def GetFeatureReport(self):
    return struct.pack('<Q', self._feature_report)


class EchoGadget(hid_gadget.HidGadget):

  def __init__(self):
    self._feature = EchoFeature()
    super(EchoGadget, self).__init__(
        report_desc=EchoFeature.REPORT_DESC,
        features={0: self._feature},
        packet_size=8,
        interval_ms=1,
        out_endpoint=True,
        vendor_id=usb_constants.VendorID.GOOGLE,
        product_id=usb_constants.ProductID.GOOGLE_HID_ECHO_GADGET,
        device_version=0x0100)
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'HID Echo Gadget')


def RegisterHandlers():
  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      server.SwitchGadget(EchoGadget())

  import server
  server.app.add_handlers('.*$', [
      (r'/hid_echo/configure', WebConfigureHandler),
  ])
