#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import hid_constants
import mouse_gadget
import usb_constants


class MouseGadgetTest(unittest.TestCase):

  def test_click(self):
    g = mouse_gadget.MouseGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.ButtonDown(hid_constants.Mouse.BUTTON_1)
    self.assertEqual(g.ControlRead(0xA1, 1, 0x0100, 0, 8), '\x01\x00\x00')
    g.ButtonUp(hid_constants.Mouse.BUTTON_1)
    chip.SendPacket.assert_has_calls([
        mock.call(0x81, '\x01\x00\x00'),
        mock.call(0x81, '\x00\x00\x00'),
    ])

  def test_move(self):
    g = mouse_gadget.MouseGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.Move(-1, 1)
    chip.SendPacket.assert_called(0x81, '\x00\xFF\x01')

  def test_drag(self):
    g = mouse_gadget.MouseGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.FULL)
    g.ButtonDown(hid_constants.Mouse.BUTTON_1)
    g.Move(5, 5)
    g.ButtonUp(hid_constants.Mouse.BUTTON_1)
    chip.SendPacket.assert_has_calls([
        mock.call(0x81, '\x01\x00\x00'),
        mock.call(0x81, '\x01\x05\x05'),
        mock.call(0x81, '\x00\x00\x00'),
    ])

if __name__ == '__main__':
  unittest.main()
