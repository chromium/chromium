#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import redirect_stdout
import io
import unittest
import setup_modules

import chromium_src.tools.metrics.histograms.print_expanded_histograms as print_expanded_histograms


class PrintExpandedHistogramsTest(unittest.TestCase):
  """Smoke tests for print_expanded_histograms.py.

  These tests run the script's main() function with real histograms.xml files,
  check for a successful exit code and perform simple validation of the output.
  """

  def testSmoke(self):
    """Tests that the script runs to completion with no arguments."""
    mock_stdout = io.StringIO()
    with redirect_stdout(mock_stdout):
      return_code = print_expanded_histograms.main([])
    self.assertEqual(0, return_code)
    output = mock_stdout.getvalue()
    # Check that the output is a valid XML with some histograms.
    self.assertIn('<histogram-configuration>', output)
    self.assertIn('</histogram-configuration>', output)
    # Check that the output contains at least one histogram.
    self.assertIn('<histogram name="', output)

  def testNamesOnly(self):
    """Tests that the script runs to completion with --print-names-only."""
    mock_stdout = io.StringIO()
    with redirect_stdout(mock_stdout):
      return_code = print_expanded_histograms.main(['--print-names-only'])
    self.assertEqual(0, return_code)
    output = mock_stdout.getvalue()
    # Check that the output is not empty and contains histogram-like names.
    self.assertTrue(output)
    self.assertIn('.', output)
    # Check that no XML tags are printed.
    self.assertNotIn('<histogram', output)


if __name__ == '__main__':
  unittest.main()
