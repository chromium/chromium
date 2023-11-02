# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of a USB HID keyboard.

Two classes are provided by this module. The KeyboardFeature class implements
the core functionality of a HID keyboard and can be included in any HID gadget.
The KeyboardGadget class implements an example keyboard gadget.
"""

import struct

import hid_constants
import hid_descriptors
import hid_gadget
import usb_constants


class KeyboardFeature(hid_gadget.HidFeature):
  """HID feature implementation for a keyboard.

  REPORT_DESC provides an example HID report descriptor for a device including
  this functionality.
  """

  REPORT_DESC = hid_descriptors.ReportDescriptor(
      hid_descriptors.UsagePage(0x01),  # Generic Desktop
      hid_descriptors.Usage(0x06),  # Keyboard
      hid_descriptors.Collection(
          hid_constants.CollectionType.APPLICATION,
          hid_descriptors.UsagePage(0x07),  # Key Codes
          hid_descriptors.UsageMinimum(224),
          hid_descriptors.UsageMaximum(231),
          hid_descriptors.LogicalMinimum(0, force_length=1),
          hid_descriptors.LogicalMaximum(1),
          hid_descriptors.ReportSize(1),
          hid_descriptors.ReportCount(8),
          hid_descriptors.Input(hid_descriptors.Data,
                                hid_descriptors.Variable,
                                hid_descriptors.Absolute),
          hid_descriptors.ReportCount(1),
          hid_descriptors.ReportSize(8),
          hid_descriptors.Input(hid_descriptors.Constant),
          hid_descriptors.ReportCount(5),
          hid_descriptors.ReportSize(1),
          hid_descriptors.UsagePage(0x08),  # LEDs
          hid_descriptors.UsageMinimum(1),
          hid_descriptors.UsageMaximum(5),
          hid_descriptors.Output(hid_descriptors.Data,
                                 hid_descriptors.Variable,
                                 hid_descriptors.Absolute),
          hid_descriptors.ReportCount(1),
          hid_descriptors.ReportSize(3),
          hid_descriptors.Output(hid_descriptors.Constant),
          hid_descriptors.ReportCount(6),
          hid_descriptors.ReportSize(8),
          hid_descriptors.LogicalMinimum(0, force_length=1),
          hid_descriptors.LogicalMaximum(101),
          hid_descriptors.UsagePage(0x07),  # Key Codes
          hid_descriptors.UsageMinimum(0, force_length=1),
          hid_descriptors.UsageMaximum(101),
          hid_descriptors.Input(hid_descriptors.Data, hid_descriptors.Array)
      )
  )

  def __init__(self):
    super(KeyboardFeature, self).__init__()
    self._modifiers = 0
    self._keys = [0, 0, 0, 0, 0, 0]
    self._leds = 0

  def ModifierDown(self, modifier):
    self._modifiers |= modifier
    if self.IsConnected():
      self.SendReport(self.GetInputReport())

  def ModifierUp(self, modifier):
    self._modifiers &= ~modifier
    if self.IsConnected():
      self.SendReport(self.GetInputReport())

  def KeyDown(self, keycode):
    free = self._keys.index(0)
    self._keys[free] = keycode
    if self.IsConnected():
      self.SendReport(self.GetInputReport())

  def KeyUp(self, keycode):
    free = self._keys.index(keycode)
    self._keys[free] = 0
    if self.IsConnected():
      self.SendReport(self.GetInputReport())

  def GetInputReport(self):
    """Construct an input report.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    Appendix B.1.

    Returns:
      A packed input report.
    """
    return struct.pack('BBBBBBBB', self._modifiers, 0, *self._keys)

  def GetOutputReport(self):
    """Construct an output report.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    Appendix B.1.

    Returns:
      A packed input report.
    """
    return struct.pack('B', self._leds)

  def SetOutputReport(self, data):
    """Handle an output report.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    Appendix B.1.

    Args:
      data: Report data.

    Returns:
      True on success, None to stall the pipe.
    """
    if len(data) >= 1:
      self._leds, = struct.unpack('B', data)
    return True


class KeyboardGadget(hid_gadget.HidGadget):
  """USB gadget implementation of a HID keyboard."""

  def __init__(self, vendor_id=0x18D1, product_id=0xFF02):
    self._feature = KeyboardFeature()
    super(KeyboardGadget, self).__init__(
        report_desc=KeyboardFeature.REPORT_DESC,
        features={0: self._feature},
        packet_size=8,
        interval_ms=1,
        out_endpoint=True,
        vendor_id=usb_constants.VendorID.GOOGLE,
        product_id=usb_constants.ProductID.GOOGLE_KEYBOARD_GADGET,
        device_version=0x0100)
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'Keyboard Gadget')

  def ModifierDown(self, modifier):
    self._feature.ModifierDown(modifier)

  def ModifierUp(self, modifier):
    self._feature.ModifierUp(modifier)

  def KeyDown(self, keycode):
    self._feature.KeyDown(keycode)

  def KeyUp(self, keycode):
    self._feature.KeyUp(keycode)


def RegisterHandlers():
  """Registers web request handlers with the application server."""

  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      server.SwitchGadget(KeyboardGadget())

  class WebTypeHandler(web.RequestHandler):

    def post(self):
      string = self.get_argument('string')
      for char in string:
        if char in hid_constants.KEY_CODES:
          code = hid_constants.KEY_CODES[char]
          server.gadget.KeyDown(code)
          server.gadget.KeyUp(code)
        elif char in hid_constants.SHIFT_KEY_CODES:
          code = hid_constants.SHIFT_KEY_CODES[char]
          server.gadget.ModifierDown(hid_constants.ModifierKey.L_SHIFT)
          server.gadget.KeyDown(code)
          server.gadget.KeyUp(code)
          server.gadget.ModifierUp(hid_constants.ModifierKey.L_SHIFT)

  class WebPressHandler(web.RequestHandler):

    def post(self):
      code = hid_constants.KEY_CODES[self.get_argument('key')]
      server.gadget.KeyDown(code)
      server.gadget.KeyUp(code)

  import server
  server.app.add_handlers('.*$', [
      (r'/keyboard/configure', WebConfigureHandler),
      (r'/keyboard/type', WebTypeHandler),
      (r'/keyboard/press', WebPressHandler),
  ])
