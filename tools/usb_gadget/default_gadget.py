# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Default gadget configuration."""

import gadget
import usb_constants
import usb_descriptors


class DefaultGadget(gadget.Gadget):

  def __init__(self):
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=usb_constants.VendorID.GOOGLE,
        idProduct=usb_constants.ProductID.GOOGLE_TEST_GADGET,
        bcdUSB=0x0200,
        iManufacturer=1,
        iProduct=2,
        iSerialNumber=3,
        bcdDevice=0x0100)

    fs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)

    hs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)

    interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=0)
    fs_config_desc.AddInterface(interface_desc)
    hs_config_desc.AddInterface(interface_desc)

    super(DefaultGadget, self).__init__(
        device_desc, fs_config_desc, hs_config_desc)

    self.AddStringDescriptor(1, "Google Inc.")
    self.AddStringDescriptor(2, "Test Gadget (default state)")
