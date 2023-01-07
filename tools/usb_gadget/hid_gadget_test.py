#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import hid_constants
import hid_descriptors
import hid_gadget
import usb_constants


report_desc = hid_descriptors.ReportDescriptor(
    hid_descriptors.UsagePage(0xFF00),  # Vendor Defined
    hid_descriptors.Usage(0x00),
    hid_descriptors.Collection(
        hid_constants.CollectionType.APPLICATION,
        hid_descriptors.LogicalMinimum(0, force_length=1),
        hid_descriptors.LogicalMaximum(255, force_length=2),
        hid_descriptors.ReportSize(8),
        hid_descriptors.ReportCount(8),
        hid_descriptors.Input(hid_descriptors.Data,
                              hid_descriptors.Variable,
                              hid_descriptors.Absolute,
                              hid_descriptors.BufferedBytes),
        hid_descriptors.Output(hid_descriptors.Data,
                               hid_descriptors.Variable,
                               hid_descriptors.Absolute,
                               hid_descriptors.BufferedBytes),
        hid_descriptors.Feature(hid_descriptors.Data,
                                hid_descriptors.Variable,
                                hid_descriptors.Absolute,
                                hid_descriptors.BufferedBytes)
    )
)

combo_report_desc = hid_descriptors.ReportDescriptor(
    hid_descriptors.ReportID(1),
    report_desc,
    hid_descriptors.ReportID(2),
    report_desc
)


class HidGadgetTest(unittest.TestCase):

  def test_bad_intervals(self):
    with self.assertRaisesRegexp(ValueError, 'Full speed'):
      hid_gadget.HidGadget(report_desc, features={}, interval_ms=50000,
                           vendor_id=0, product_id=0)
    with self.assertRaisesRegexp(ValueError, 'High speed'):
      hid_gadget.HidGadget(report_desc, features={}, interval_ms=5000,
                           vendor_id=0, product_id=0)

  def test_get_string_descriptor(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.AddStringDescriptor(2, 'HID Gadget')
    desc = g.ControlRead(0x80, 6, 0x0302, 0x0409, 255)
    self.assertEquals(desc, '\x16\x03H\0I\0D\0 \0G\0a\0d\0g\0e\0t\0')

  def test_get_report_descriptor(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    desc = g.ControlRead(0x81, 6, 0x2200, 0, 63)
    self.assertEquals(desc, report_desc)

  def test_set_idle(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0x21, 0x0A, 0, 0, ''))

  def test_class_wrong_target(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertIsNone(g.ControlRead(0xA0, 0, 0, 0, 0))  # Device
    self.assertIsNone(g.ControlRead(0xA1, 0, 0, 1, 0))  # Interface 1
    self.assertIsNone(g.ControlWrite(0x20, 0, 0, 0, ''))  # Device
    self.assertIsNone(g.ControlWrite(0x21, 0, 0, 1, ''))  # Interface 1

  def test_send_report_zero(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SendReport(0, 'Hello world!')
    chip.SendPacket.assert_called_once_with(0x81, 'Hello world!')

  def test_send_multiple_reports(self):
    g = hid_gadget.HidGadget(report_desc=report_desc, features={},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SendReport(1, 'Hello!')
    g.SendReport(2, 'World!')
    chip.SendPacket.assert_has_calls([
        mock.call(0x81, '\x01Hello!'),
        mock.call(0x81, '\x02World!'),
    ])


class TestFeature(hid_gadget.HidFeature):

  def SetInputReport(self, data):
    self.input_report = data
    return True

  def SetOutputReport(self, data):
    self.output_report = data
    return True

  def SetFeatureReport(self, data):
    self.feature_report = data
    return True

  def GetInputReport(self):
    return 'Input report.'

  def GetOutputReport(self):
    return 'Output report.'

  def GetFeatureReport(self):
    return 'Feature report.'


class HidFeatureTest(unittest.TestCase):

  def test_disconnected(self):
    feature = TestFeature()
    with self.assertRaisesRegexp(RuntimeError, 'not connected'):
      feature.SendReport('Hello world!')

  def test_send_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    feature.SendReport('Hello world!')
    chip.SendPacket.assert_called_once_with(0x81, '\x01Hello world!')
    g.Disconnected()

  def test_get_bad_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertIsNone(g.ControlRead(0xA1, 1, 0x0102, 0, 8))

  def test_set_bad_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertIsNone(g.ControlWrite(0x21, 0x09, 0x0102, 0, 'Hello!'))

  def test_get_input_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    report = g.ControlRead(0xA1, 1, 0x0101, 0, 8)
    self.assertEquals(report, 'Input re')

  def test_set_input_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0x21, 0x09, 0x0101, 0, 'Hello!'))
    self.assertEquals(feature.input_report, 'Hello!')

  def test_get_output_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    report = g.ControlRead(0xA1, 1, 0x0201, 0, 8)
    self.assertEquals(report, 'Output r')

  def test_set_output_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0x21, 0x09, 0x0201, 0, 'Hello!'))
    self.assertEquals(feature.output_report, 'Hello!')

  def test_receive_interrupt(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)
    g.ReceivePacket(0x01, '\x01Hello!')
    self.assertFalse(chip.HaltEndpoint.called)
    self.assertEquals(feature.output_report, 'Hello!')

  def test_receive_interrupt_report_zero(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={0: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)
    g.ReceivePacket(0x01, 'Hello!')
    self.assertFalse(chip.HaltEndpoint.called)
    self.assertEquals(feature.output_report, 'Hello!')

  def test_receive_bad_interrupt(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)
    g.ReceivePacket(0x01, '\x00Hello!')
    chip.HaltEndpoint.assert_called_once_with(0x01)

  def test_get_feature_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    report = g.ControlRead(0xA1, 1, 0x0301, 0, 8)
    self.assertEquals(report, 'Feature ')

  def test_set_feature_report(self):
    feature = TestFeature()
    g = hid_gadget.HidGadget(report_desc, features={1: feature},
                             vendor_id=0, product_id=0)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0x21, 0x09, 0x0301, 0, 'Hello!'))
    self.assertEquals(feature.feature_report, 'Hello!')


if __name__ == '__main__':
  unittest.main()
