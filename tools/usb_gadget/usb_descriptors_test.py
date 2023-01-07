#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import hid_constants
import usb_descriptors


class DescriptorWithField(usb_descriptors.Descriptor):
  pass

DescriptorWithField.AddField('bField', 'B')


class DescriptorWithDefault(usb_descriptors.Descriptor):
  pass

DescriptorWithDefault.AddField('bDefault', 'B', default=42)


class DescriptorWithFixed(usb_descriptors.Descriptor):
  pass

DescriptorWithFixed.AddFixedField('bFixed', 'B', 42)


class DescriptorWithComputed(usb_descriptors.Descriptor):

  @property
  def foo(self):
    return 42

DescriptorWithComputed.AddComputedField('bComputed', 'B', 'foo')


class DescriptorWithDescriptors(usb_descriptors.DescriptorContainer):
  pass

DescriptorWithDescriptors.AddField('bType', 'B')


class DescriptorTest(unittest.TestCase):

  def test_default(self):
    obj = DescriptorWithDefault()
    self.assertEquals(obj.bDefault, 42)

  def test_change_default(self):
    obj = DescriptorWithDefault()
    obj.bDefault = 1
    self.assertEquals(obj.bDefault, 1)

  def test_override_default(self):
    obj = DescriptorWithDefault(bDefault=56)
    self.assertEquals(obj.bDefault, 56)

  def test_fixed(self):
    obj = DescriptorWithFixed()
    self.assertEquals(obj.bFixed, 42)

  def test_set_fixed(self):
    with self.assertRaises(RuntimeError):
      DescriptorWithFixed(bFixed=1)

  def test_modify_fixed(self):
    obj = DescriptorWithFixed()
    with self.assertRaises(RuntimeError):
      obj.bFixed = 1

  def test_computed(self):
    obj = DescriptorWithComputed()
    self.assertEquals(obj.bComputed, 42)

  def test_set_computed(self):
    with self.assertRaises(RuntimeError):
      DescriptorWithComputed(bComputed=1)

  def test_modify_computed(self):
    obj = DescriptorWithComputed()
    with self.assertRaises(RuntimeError):
      obj.bComputed = 1

  def test_unexpected(self):
    with self.assertRaisesRegexp(TypeError, 'Unexpected'):
      DescriptorWithField(bUnexpected=1)

  def test_missing(self):
    with self.assertRaisesRegexp(TypeError, 'Missing'):
      DescriptorWithField()

  def test_size(self):
    obj = DescriptorWithField(bField=42)
    self.assertEquals(obj.struct_size, 1)
    self.assertEquals(obj.total_size, 1)

  def test_encode(self):
    obj = DescriptorWithField(bField=0xff)
    self.assertEquals(obj.Encode(), '\xff')

  def test_string(self):
    obj = DescriptorWithField(bField=42)
    string = str(obj)
    self.assertIn('bField', string)
    self.assertIn('42', string)

  def test_container(self):
    parent = DescriptorWithDescriptors(bType=0)
    child1 = DescriptorWithField(bField=1)
    parent.Add(child1)
    child2 = DescriptorWithField(bField=2)
    parent.Add(child2)
    self.assertEquals(parent.total_size, 3)
    self.assertEquals(parent.Encode(), '\x00\x01\x02')
    string = str(parent)
    self.assertIn('bType', string)
    self.assertIn('bField', string)


class TestUsbDescriptors(unittest.TestCase):

  def test_device_descriptor(self):
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=0xDEAD,
        idProduct=0xBEEF,
        bcdDevice=0x0100,
        bNumConfigurations=1)
    self.assertEquals(
        device_desc.Encode(),
        '\x12\x01\x00\x02\x00\x00\x00\x40\xAD\xDE\xEF\xBE\x00\x01\x00\x00\x00'
        '\x01')

  def test_unique_interfaces(self):
    interface_desc1 = usb_descriptors.InterfaceDescriptor(bInterfaceNumber=1)
    interface_desc2 = usb_descriptors.InterfaceDescriptor(bInterfaceNumber=1,
                                                          bAlternateSetting=1)
    interface_desc3 = usb_descriptors.InterfaceDescriptor(bInterfaceNumber=1)

    configuration_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0xC0,
        MaxPower=100)
    configuration_desc.AddInterface(interface_desc1)
    configuration_desc.AddInterface(interface_desc2)
    with self.assertRaisesRegexp(RuntimeError, r'Interface 1 \(alternate 0\)'):
      configuration_desc.AddInterface(interface_desc3)

  def test_unique_endpoints(self):
    endpoint_desc1 = usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x01,
        bmAttributes=0x02,
        wMaxPacketSize=64,
        bInterval=1)
    endpoint_desc2 = usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x81,
        bmAttributes=0x02,
        wMaxPacketSize=64,
        bInterval=1)
    endpoint_desc3 = usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x01,
        bmAttributes=0x01,
        wMaxPacketSize=32,
        bInterval=10)

    interface_desc = usb_descriptors.InterfaceDescriptor(bInterfaceNumber=1)
    interface_desc.AddEndpoint(endpoint_desc1)
    interface_desc.AddEndpoint(endpoint_desc2)
    with self.assertRaisesRegexp(RuntimeError, 'Endpoint 0x01 already defined'):
      interface_desc.AddEndpoint(endpoint_desc3)

  def test_configuration_descriptor(self):
    endpoint_desc = usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x01,
        bmAttributes=0x02,
        wMaxPacketSize=64,
        bInterval=1)
    encoded_endpoint = '\x07\x05\x01\x02\x40\x00\x01'
    self.assertEquals(endpoint_desc.Encode(), encoded_endpoint)

    interface_desc = usb_descriptors.InterfaceDescriptor(bInterfaceNumber=1)
    interface_desc.AddEndpoint(endpoint_desc)
    self.assertEquals([endpoint_desc], interface_desc.GetEndpoints())
    encoded_interface = ('\x09\x04\x01\x00\x01\xFF\xFF\xFF\x00' +
                         encoded_endpoint)
    self.assertEquals(interface_desc.Encode(), encoded_interface)

    configuration_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0xC0,
        MaxPower=100)
    configuration_desc.AddInterface(interface_desc)
    self.assertEquals([interface_desc], configuration_desc.GetInterfaces())
    encoded_configuration = ('\x09\x02\x19\x00\x01\x01\x00\xC0\x64' +
                             encoded_interface)
    self.assertEquals(configuration_desc.Encode(), encoded_configuration)

  def test_encode_hid_descriptor(self):
    hid_desc = usb_descriptors.HidDescriptor()
    hid_desc.AddDescriptor(hid_constants.DescriptorType.REPORT, 0x80)
    hid_desc.AddDescriptor(hid_constants.DescriptorType.PHYSICAL, 0x60)
    encoded_desc = '\x0C\x21\x11\x01\x00\x02\x22\x80\x00\x23\x60\x00'
    self.assertEquals(hid_desc.Encode(), encoded_desc)

  def test_print_hid_descriptor(self):
    hid_desc = usb_descriptors.HidDescriptor()
    hid_desc.AddDescriptor(hid_constants.DescriptorType.REPORT, 0x80)
    hid_desc.AddDescriptor(hid_constants.DescriptorType.PHYSICAL, 0x60)
    string = str(hid_desc)
    self.assertIn('0x22', string)
    self.assertIn('0x23', string)


if __name__ == '__main__':
  unittest.main()
