#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import unittest

import validate_token


class ValidateTokenTests(unittest.TestCase):
  def test_valid_tokens(self):
    # Hacky way to verify no log is emitted.
    # TODO(arielzhang): Use assertNoLogs instead when Python 3.10 is supported.
    with self.assertLogs() as logs:
      logging.info('ensure non-empty log')
      has_token_error = validate_token.ValidateTokenInFile(
          f'{os.path.dirname(__file__)}/test_data/histograms.xml')
      self.assertFalse(has_token_error)
    self.assertEqual(len(logs.output), 1)

  def test_invalid_tokens(self):
    with self.assertLogs() as logs:
      has_token_error = validate_token.ValidateTokenInFile(
          f'{os.path.dirname(__file__)}/test_data/tokens/histograms.xml')
      self.assertTrue(has_token_error)
    self.assertEqual(len(logs.output), 1)
    output = logs.output[0]
    self.assertIn('Token(s) TestToken, TestToken3 in', output)
    self.assertIn('Test.{TestToken3}.Histogram.{TestToken}.{TestToken2}',
                  output)


if __name__ == '__main__':
  unittest.main()
