#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import echo_gadget
import usb_constants


class EchoGadgetTest(unittest.TestCase):

  def test_bulk_echo(self):
    g = echo_gadget.EchoGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)
    g.ReceivePacket(0x02, 'Hello world!')
    chip.SendPacket.assert_called_once_with(0x82, 'Hello world!')
