#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import private_metrics_codegen
import dkm_builders_template
import dkm_model
import dwa_builders_template
import dwa_decode_template
import dwa_model

_FILE_DIR = os.path.dirname(__file__)


class GenBuildersDkmTest(unittest.TestCase):

  def setUp(self) -> None:
    self.relpath = 'test-relpath/'
    with open(_FILE_DIR + '/tests/dkm/dkm_test.xml') as f:
      self.data = dkm_model.DKM_XML_TYPE.Parse(f.read())
    with open(_FILE_DIR + '/tests/dkm/expected_output/dkm_builders.h.out') as f:
      self.expected_header = f.read()
    with open(_FILE_DIR +
              '/tests/dkm/expected_output/dkm_builders.cc.out') as f:
      self.expected_impl = f.read()

  def testBuildersHeaderOutput(self) -> None:
    builders_header_output = dkm_builders_template.HEADER._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(builders_header_output, self.expected_header)

  def testBuildersImplOutput(self) -> None:
    builders_impl_output = dkm_builders_template.IMPL._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(builders_impl_output, self.expected_impl)


class GenBuildersDwaTest(unittest.TestCase):

  def setUp(self) -> None:
    self.relpath = 'test-relpath/'
    with open(_FILE_DIR + '/tests/dwa/dwa_test.xml') as f:
      self.data = dwa_model.DWA_XML_TYPE.Parse(f.read())
    with open(_FILE_DIR + '/tests/dwa/expected_output/dwa_builders.h.out') as f:
      self.expected_header = f.read()
    with open(_FILE_DIR +
              '/tests/dwa/expected_output/dwa_builders.cc.out') as f:
      self.expected_impl = f.read()
    with open(_FILE_DIR + '/tests/dwa/expected_output/dwa_decode.h.out') as f:
      self.expected_decode_header = f.read()
    with open(_FILE_DIR + '/tests/dwa/expected_output/dwa_decode.cc.out') as f:
      self.expected_decode_impl = f.read()

  def testBuildersHeaderOutput(self) -> None:
    builders_header_output = dwa_builders_template.HEADER._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(builders_header_output, self.expected_header)

  def testBuildersImplOutput(self) -> None:
    builders_impl_output = dwa_builders_template.IMPL._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(builders_impl_output, self.expected_impl)

  def testDecodeHeaderOutput(self) -> None:
    decode_header_output = dwa_decode_template.HEADER._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(decode_header_output, self.expected_decode_header)

  def testDecodeImplOutput(self) -> None:
    decode_impl_output = dwa_decode_template.IMPL._StampFileCode(
        self.relpath, self.data)
    self.assertEqual(decode_impl_output, self.expected_decode_impl)


if __name__ == '__main__':
  unittest.main()
