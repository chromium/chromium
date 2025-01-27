# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import unittest

import sys

# Force local directory to be in front of sys.path to avoid importing different
# version of PRESUBMIT.py which can be added in different files within python
# invocation.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import PRESUBMIT

# Append chrome source root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi


_BASE_DIR = os.path.dirname(os.path.abspath(__file__))

class MetricsPresubmitTest(unittest.TestCase):

  def testCheckHistogramFormattingFailureIsDetected(self):
    malformed_histograms_path = (
        f'{os.path.dirname(__file__)}'
        '/test_data/tokens/token_errors_histograms.xml')

    with open(malformed_histograms_path, 'r') as f:
      malformed_contents = f.read()
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(malformed_histograms_path, malformed_contents),
    ]

    results = PRESUBMIT.CheckHistogramFormatting(mock_input_api,
                                                 MockOutputApi(),
                                                 allow_test_paths=True)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertRegex(
        results[0].message,
        '.*histograms.xml contains histogram.* using <variants> not defined in'
        ' the file, please run validate_token.py .*histograms.xml to fix.')

  def testCheckWebViewHistogramsAllowlistOnUploadFailureIsDetected(self):
    missing_allow_list_entries_histograms_path = (
        f'{os.path.dirname(__file__)}'
        '/test_data'
        '/no_allowlist_entries_histograms.xml')

    with open(missing_allow_list_entries_histograms_path, 'r') as f:
      invalid_histograms_contents = f.read()
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(missing_allow_list_entries_histograms_path,
                         invalid_histograms_contents),
    ]

    results = PRESUBMIT.CheckWebViewHistogramsAllowlistOnUpload(
        mock_input_api,
        MockOutputApi(),
        xml_paths_override=[missing_allow_list_entries_histograms_path],
    )
    self.assertEqual(len(results), 1)
    self.assertRegex(
        results[0].message.replace('\n', ' '), 'All histograms in'
        ' .*histograms_allowlist.txt must be valid.')
    self.assertEqual(results[0].type, 'error')

  def testCheckBooleansAreEnumsFailureIsDetected(self):
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile('histograms.xml',
                         ['<histogram name="Foo" units="Boolean" />']),
    ]

    results = PRESUBMIT.CheckBooleansAreEnums(mock_input_api, MockOutputApi())
    self.assertEqual(len(results), 1)
    self.assertRegex(
        results[0].message.replace('\n', ' '),
        '.*You are using .units. for a boolean histogram, but you should be'
        ' using\s+.enum. instead\.')
    self.assertEqual(results[0].type, 'promptOrNotify')

  def testCheckHistogramFormattingPasses(self):
    valid_histograms_path = (f'{os.path.dirname(__file__)}'
                             '/test_data'
                             '/example_valid_histograms.xml')

    with open(valid_histograms_path, 'r') as f:
      valid_contents = f.read()

    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(valid_histograms_path, valid_contents),
    ]

    results = PRESUBMIT.CheckHistogramFormatting(mock_input_api,
                                                 MockOutputApi(),
                                                 allow_test_paths=True)
    self.assertEqual(len(results), 0)

  def testCheckWebViewHistogramsAllowlistOnUploadPasses(self):
    valid_histograms_path = (f'{os.path.dirname(__file__)}'
                             '/test_data'
                             '/example_valid_histograms.xml')

    with open(valid_histograms_path, 'r') as f:
      valid_contents = f.read()

    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(valid_histograms_path, valid_contents),
    ]

    results = PRESUBMIT.CheckWebViewHistogramsAllowlistOnUpload(
        mock_input_api, MockOutputApi())
    self.assertEqual(len(results), 0)

  def testCheckBooleansAreEnumsPasses(self):
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile('histograms.xml',
                         ['<histogram name="Foo" enum="Boolean" />']),
    ]

    results = PRESUBMIT.CheckBooleansAreEnums(mock_input_api, MockOutputApi())
    self.assertEqual(len(results), 0)


if __name__ == '__main__':
  unittest.main()
