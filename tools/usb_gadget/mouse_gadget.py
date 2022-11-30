# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of a USB HID mouse.

Two classes are provided by this module. The MouseFeature class implements
the core functionality of a HID mouse and can be included in any HID gadget.
The MouseGadget class implements an example mouse gadget.
"""

import struct

import hid_constants
import hid_descriptors
import hid_gadget
import usb_constants


class MouseFeature(hid_gadget.HidFeature):
  """HID feature implementation for a mouse.

  REPORT_DESC provides an example HID report descriptor for a device including
  this functionality.
  """

  REPORT_DESC = hid_descriptors.ReportDescriptor(
      hid_descriptors.UsagePage(0x01),  # Generic Desktop
      hid_descriptors.Usage(0x02),  # Mouse
      hid_descriptors.Collection(
          hid_constants.CollectionType.APPLICATION,
          hid_descriptors.Usage(0x01),  # Pointer
          hid_descriptors.Collection(
              hid_constants.CollectionType.PHYSICAL,
              hid_descriptors.UsagePage(0x09),  # Buttons
              hid_descriptors.UsageMinimum(1),
              hid_descriptors.UsageMaximum(3),
              hid_descriptors.LogicalMinimum(0, force_length=1),
              hid_descriptors.LogicalMaximum(1),
              hid_descriptors.ReportCount(3),
              hid_descriptors.ReportSize(1),
              hid_descriptors.Input(hid_descriptors.Data,
                                    hid_descriptors.Variable,
                                    hid_descriptors.Absolute),
              hid_descriptors.ReportCount(1),
              hid_descriptors.ReportSize(5),
              hid_descriptors.Input(hid_descriptors.Constant),
              hid_descriptors.UsagePage(0x01),  # Generic Desktop
              hid_descriptors.Usage(0x30),  # X
              hid_descriptors.Usage(0x31),  # Y
              hid_descriptors.LogicalMinimum(0x81),  # -127
              hid_descriptors.LogicalMaximum(127),
              hid_descriptors.ReportSize(8),
              hid_descriptors.ReportCount(2),
              hid_descriptors.Input(hid_descriptors.Data,
                                    hid_descriptors.Variable,
                                    hid_descriptors.Relative)
          )
      )
  )

  def __init__(self):
    super(MouseFeature, self).__init__()
    self._buttons = 0

  def ButtonDown(self, button):
    self._buttons |= button
    if self.IsConnected():
      self.SendReport(self.EncodeInputReport())

  def ButtonUp(self, button):
    self._buttons &= ~button
    if self.IsConnected():
      self.SendReport(self.EncodeInputReport())

  def Move(self, x_displacement, y_displacement):
    if self.IsConnected():
      self.SendReport(self.EncodeInputReport(x_displacement, y_displacement))

  def EncodeInputReport(self, x_displacement=0, y_displacement=0):
    return struct.pack('Bbb', self._buttons, x_displacement, y_displacement)

  def GetInputReport(self):
    """Construct an input report.

    See Device Class Definition for Human Interface Devices (HID) Version 1.11
    Appendix B.2.

    Returns:
      A packed input report.
    """
    return self.EncodeInputReport()


class MouseGadget(hid_gadget.HidGadget):
  """USB gadget implementation of a HID mouse."""

  def __init__(self):
    self._feature = MouseFeature()
    super(MouseGadget, self).__init__(
        report_desc=MouseFeature.REPORT_DESC,
        features={0: self._feature},
        packet_size=8,
        interval_ms=1,
        out_endpoint=False,
        vendor_id=usb_constants.VendorID.GOOGLE,
        product_id=usb_constants.ProductID.GOOGLE_MOUSE_GADGET,
        device_version=0x0100)
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'Mouse Gadget')

  def ButtonDown(self, button):
    self._feature.ButtonDown(button)

  def ButtonUp(self, button):
    self._feature.ButtonUp(button)

  def Move(self, x_displacement, y_displacement):
    self._feature.Move(x_displacement, y_displacement)


def RegisterHandlers():
  """Registers web request handlers with the application server."""

  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      gadget = MouseGadget()
      server.SwitchGadget(gadget)

  class WebClickHandler(web.RequestHandler):

    def post(self):
      BUTTONS = {
          '1': hid_constants.Mouse.BUTTON_1,
          '2': hid_constants.Mouse.BUTTON_2,
          '3': hid_constants.Mouse.BUTTON_3,
      }

      button = BUTTONS[self.get_argument('button')]
      server.gadget.ButtonDown(button)
      server.gadget.ButtonUp(button)

  class WebMoveHandler(web.RequestHandler):

    def post(self):
      x = int(self.get_argument('x'))
      y = int(self.get_argument('y'))
      server.gadget.Move(x, y)

  import server
  server.app.add_handlers('.*$', [
      (r'/mouse/configure', WebConfigureHandler),
      (r'/mouse/move', WebMoveHandler),
      (r'/mouse/click', WebClickHandler),
  ])
