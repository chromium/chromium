#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.data_pack'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit.format import data_pack


class FormatDataPackUnittest(unittest.TestCase):
  def testReadDataPackV4(self):
    expected_data = (
        b'\x04\x00\x00\x00'                  # header(version
        b'\x04\x00\x00\x00'                  #        no. entries,
        b'\x01'                              #        encoding)
        b'\x01\x00\x27\x00\x00\x00'          # index entry 1
        b'\x04\x00\x27\x00\x00\x00'          # index entry 4
        b'\x06\x00\x33\x00\x00\x00'          # index entry 6
        b'\x0a\x00\x3f\x00\x00\x00'          # index entry 10
        b'\x00\x00\x3f\x00\x00\x00'          # extra entry for the size of last
        b'this is id 4this is id 6')         # data
    expected_data_pack = data_pack.DataPackContents(
        {
            1: b'',
            4: b'this is id 4',
            6: b'this is id 6',
            10: b'',
        }, data_pack.UTF8, 4, {}, data_pack.DataPackSizes(9, 30, 0, 24))
    loaded = data_pack.ReadDataPackFromString(expected_data)
    self.assertDictEqual(expected_data_pack.__dict__, loaded.__dict__)

  def testReadWriteDataPackV5(self):
    expected_data = (
        b'\x05\x00\x00\x00'                  # version
        b'\x01\x00\x00\x00'                  # encoding & padding
        b'\x03\x00'                          # resource_count
        b'\x01\x00'                          # alias_count
        b'\x01\x00\x28\x00\x00\x00'          # index entry 1
        b'\x04\x00\x28\x00\x00\x00'          # index entry 4
        b'\x06\x00\x34\x00\x00\x00'          # index entry 6
        b'\x00\x00\x40\x00\x00\x00'          # extra entry for the size of last
        b'\x0a\x00\x01\x00'                  # alias table
        b'this is id 4this is id 6')         # data
    input_resources = {
        1: b'',
        4: b'this is id 4',
        6: b'this is id 6',
        10: b'this is id 4',
    }
    data = data_pack.WriteDataPackToString(input_resources, data_pack.UTF8)
    self.assertEqual(data, expected_data)

    expected_data_pack = data_pack.DataPackContents({
        1: b'',
        4: input_resources[4],
        6: input_resources[6],
        10: input_resources[4],
    }, data_pack.UTF8, 5, {10: 4}, data_pack.DataPackSizes(12, 24, 4, 24))
    loaded = data_pack.ReadDataPackFromString(expected_data)
    self.assertDictEqual(expected_data_pack.__dict__, loaded.__dict__)

  def testRePackUnittest(self):
    expected_with_allowlist = {
        1: 'Never gonna',
        10: 'give you up',
        20: 'Never gonna let',
        30: 'you down',
        40: 'Never',
        50: 'gonna run around and',
        60: 'desert you'
    }
    expected_without_allowlist = {
        1: 'Never gonna',
        10: 'give you up',
        20: 'Never gonna let',
        65: 'Close',
        30: 'you down',
        40: 'Never',
        50: 'gonna run around and',
        4: 'click',
        60: 'desert you',
        6: 'chirr',
        32: 'oops, try again',
        70: 'Awww, snap!'
    }
    inputs = [{1: 'Never gonna', 4: 'click', 6: 'chirr', 10: 'give you up'},
              {20: 'Never gonna let', 30: 'you down', 32: 'oops, try again'},
              {40: 'Never', 50: 'gonna run around and', 60: 'desert you'},
              {65: 'Close', 70: 'Awww, snap!'}]
    allowlist = [1, 10, 20, 30, 40, 50, 60]
    inputs = [(i, data_pack.UTF8) for i in inputs]

    # RePack using allowlist
    output, _ = data_pack.RePackFromDataPackStrings(
        inputs, allowlist, suppress_removed_key_output=True)
    self.assertDictEqual(expected_with_allowlist, output,
                         'Incorrect resource output')

    # RePack a None allowlist
    output, _ = data_pack.RePackFromDataPackStrings(
        inputs, None, suppress_removed_key_output=True)
    self.assertDictEqual(expected_without_allowlist, output,
                         'Incorrect resource output')


if __name__ == '__main__':
  unittest.main()
