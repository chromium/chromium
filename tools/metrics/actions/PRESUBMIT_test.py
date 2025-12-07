# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest.mock import MagicMock, patch
import PRESUBMIT
import sys


class PresubmitTest(unittest.TestCase):

  def testCheckRemovedSegmentationUserActions(self):
    mock_input_api = MagicMock()
    mock_output_api = MagicMock()

    # Create a mock error object to be returned by PresubmitError
    mock_error = MagicMock()
    mock_output_api.PresubmitError.return_value = mock_error

    mock_file = MagicMock()
    mock_file.LocalPath.return_value = 'tools/metrics/actions/actions.xml'
    mock_input_api.AffectedFiles.return_value = [mock_file]

    # Mock the imported module
    mock_print_action_names = MagicMock()
    mock_print_action_names.get_action_diff.return_value = ([], [
        'Test.Action.Removed'
    ])

    # Add the mock to sys.modules
    sys.modules['print_action_names'] = mock_print_action_names

    mock_generate_histogram_list = MagicMock()
    mock_generate_histogram_list.GetActualActionNames.return_value = {
        'Test.Action.Removed'
    }
    sys.modules['generate_histogram_list'] = mock_generate_histogram_list

    errors = PRESUBMIT.CheckRemovedSegmentationUserActions(
        mock_input_api, mock_output_api)

    # Clean up sys.modules
    del sys.modules['print_action_names']
    del sys.modules['generate_histogram_list']

    self.assertEqual(len(errors), 1)
    # Check that PresubmitError was called with the correct message
    mock_output_api.PresubmitError.assert_called_once()
    self.assertIn(
        'The following user actions are used by segmentation platform',
        mock_output_api.PresubmitError.call_args[0][0])


if __name__ == '__main__':
  unittest.main()
