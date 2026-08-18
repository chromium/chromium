#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest

import setup_modules  # pylint: disable=unused-import

from chromium_src.tools.metrics.common import path_util
import chromium_src.tools.metrics.histograms.validate_token as validate_token


_BASE_PATH = path_util.METRICS_TOOLS_PATH / 'histograms' / 'test_data'


class ValidateTokenTests(unittest.TestCase):
  def test_valid_tokens(self):
    # Hacky way to verify no log is emitted.
    # TODO(arielzhang): Use assertNoLogs instead when Python 3.10 is supported.
    with self.assertLogs() as logs:
      logging.info('ensure non-empty log')
      has_token_error = validate_token.ValidateTokenInFile(
        str(_BASE_PATH / 'histograms.xml')
      )
      self.assertFalse(has_token_error)
    self.assertEqual(len(logs.output), 1)

  def test_invalid_tokens(self):
    with self.assertLogs() as logs:
      has_token_error = validate_token.ValidateTokenInFile(
        str(_BASE_PATH / 'tokens' / 'token_errors_histograms.xml')
      )
      self.assertTrue(has_token_error)
    self.assertEqual(len(logs.output), 1)
    output = logs.output[0]
    self.assertIn('Token(s) TestToken3 in', output)
    self.assertIn(
      'Test.{TestToken3}.Histogram.{TestToken}.{TestToken2}', output
    )


if __name__ == '__main__':
  unittest.main()
