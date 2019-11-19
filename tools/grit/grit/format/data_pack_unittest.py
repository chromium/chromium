#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.data_pack'''

from __future__ import print_function

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit.format import data_pack


class FormatDataPackUnittest(unittest.TestCase):
  def testReadDataPackV4(self):
    expected_data = (
        '\x04\x00\x00\x00'                  # header(version
        '\x04\x00\x00\x00'                  #        no. entries,
        '\x01'                              #        encoding)
        '\x01\x00\x27\x00\x00\x00'          # index entry 1
        '\x04\x00\x27\x00\x00\x00'          # index entry 4
        '\x06\x00\x33\x00\x00\x00'          # index entry 6
        '\x0a\x00\x3f\x00\x00\x00'          # index entry 10
        '\x00\x00\x3f\x00\x00\x00'          # extra entry for the size of last
        'this is id 4this is id 6')         # data
    expected_data_pack = data_pack.DataPackContents(
        {
            1: '',
            4: 'this is id 4',
            6: 'this is id 6',
            10: '',
        }, data_pack.UTF8, 4, {}, data_pack.DataPackSizes(9, 30, 0, 24))
    loaded = data_pack.ReadDataPackFromString(expected_data)
    self.assertDictEqual(expected_data_pack.__dict__, loaded.__dict__)

  def testReadWriteDataPackV5(self):
    expected_data = (
        '\x05\x00\x00\x00'                  # version
        '\x01\x00\x00\x00'                  # encoding & padding
        '\x03\x00'                          # resource_count
        '\x01\x00'                          # alias_count
        '\x01\x00\x28\x00\x00\x00'          # index entry 1
        '\x04\x00\x28\x00\x00\x00'          # index entry 4
        '\x06\x00\x34\x00\x00\x00'          # index entry 6
        '\x00\x00\x40\x00\x00\x00'          # extra entry for the size of last
        '\x0a\x00\x01\x00'                  # alias table
        'this is id 4this is id 6')         # data
    input_resources = {
        1: '',
        4: 'this is id 4',
        6: 'this is id 6',
        10: 'this is id 4',
    }
    data = data_pack.WriteDataPackToString(input_resources, data_pack.UTF8)
    self.assertEquals(data, expected_data)

    expected_data_pack = data_pack.DataPackContents(
        {
            1: '',
            4: input_resources[4],
            6: input_resources[6],
            10: input_resources[4],
        }, data_pack.UTF8, 5, {10: 4}, data_pack.DataPackSizes(12, 24, 4, 24))
    loaded = data_pack.ReadDataPackFromString(expected_data)
    self.assertDictEqual(expected_data_pack.__dict__, loaded.__dict__)

  def testRePackUnittest(self):
    expected_with_whitelist = {
        1: 'Never gonna', 10: 'give you up', 20: 'Never gonna let',
        30: 'you down', 40: 'Never', 50: 'gonna run around and',
        60: 'desert you'}
    expected_without_whitelist = {
        1: 'Never gonna', 10: 'give you up', 20: 'Never gonna let', 65: 'Close',
        30: 'you down', 40: 'Never', 50: 'gonna run around and', 4: 'click',
        60: 'desert you', 6: 'chirr', 32: 'oops, try again', 70: 'Awww, snap!'}
    inputs = [{1: 'Never gonna', 4: 'click', 6: 'chirr', 10: 'give you up'},
              {20: 'Never gonna let', 30: 'you down', 32: 'oops, try again'},
              {40: 'Never', 50: 'gonna run around and', 60: 'desert you'},
              {65: 'Close', 70: 'Awww, snap!'}]
    whitelist = [1, 10, 20, 30, 40, 50, 60]
    inputs = [(i, data_pack.UTF8) for i in inputs]

    # RePack using whitelist
    output, _ = data_pack.RePackFromDataPackStrings(
        inputs, whitelist, suppress_removed_key_output=True)
    self.assertDictEqual(expected_with_whitelist, output,
                         'Incorrect resource output')

    # RePack a None whitelist
    output, _ = data_pack.RePackFromDataPackStrings(
        inputs, None, suppress_removed_key_output=True)
    self.assertDictEqual(expected_without_whitelist, output,
                         'Incorrect resource output')


if __name__ == '__main__':
  unittest.main()
