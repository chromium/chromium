#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import hid_constants
import keyboard_gadget
import usb_constants


class KeyboardGadgetTest(unittest.TestCase):

  def test_key_press(self):
    g = keyboard_gadget.KeyboardGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.KeyDown(0x04)
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0100, 0, 8),
                     '\x00\x00\x04\x00\x00\x00\x00\x00')
    g.KeyUp(0x04)
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0100, 0, 8),
                     '\x00\x00\x00\x00\x00\x00\x00\x00')
    chip.SendPacket.assert_has_calls([
        mock.call(0x81, '\x00\x00\x04\x00\x00\x00\x00\x00'),
        mock.call(0x81, '\x00\x00\x00\x00\x00\x00\x00\x00'),
    ])

  def test_key_press_with_modifier(self):
    g = keyboard_gadget.KeyboardGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.ModifierDown(hid_constants.ModifierKey.L_SHIFT)
    g.KeyDown(0x04)
    g.KeyDown(0x05)
    g.KeyUp(0x04)
    g.KeyUp(0x05)
    g.ModifierUp(hid_constants.ModifierKey.L_SHIFT)
    chip.SendPacket.assert_has_calls([
        mock.call(0x81, '\x02\x00\x00\x00\x00\x00\x00\x00'),
        mock.call(0x81, '\x02\x00\x04\x00\x00\x00\x00\x00'),
        mock.call(0x81, '\x02\x00\x04\x05\x00\x00\x00\x00'),
        mock.call(0x81, '\x02\x00\x00\x05\x00\x00\x00\x00'),
        mock.call(0x81, '\x02\x00\x00\x00\x00\x00\x00\x00'),
        mock.call(0x81, '\x00\x00\x00\x00\x00\x00\x00\x00'),
    ])

  def test_set_leds(self):
    g = keyboard_gadget.KeyboardGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.SetConfiguration(1)
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0200, 0, 8), '\x00')
    self.assertTrue(g.ControlWrite(0x21, 9, 0x0200, 0, '\x01'))
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0200, 0, 8), '\x01')
    g.ReceivePacket(0x01, '\x03')
    self.assertFalse(chip.HaltEndpoint.called)
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0200, 0, 8), '\x03')

if __name__ == '__main__':
  unittest.main()
