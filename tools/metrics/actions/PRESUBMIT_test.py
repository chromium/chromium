# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest.mock import MagicMock, patch
import sys
import os
import tempfile

import setup_modules

import chromium_src.tools.metrics.actions.PRESUBMIT as PRESUBMIT

_CONFLICTING_NAME_1 = "UserEducation.MessageAction.Cancel" \
                      ".IPH_ScalableIphUnlockedBasedOne"
_CONFLICTING_NAME_2 = "SidePanel.ShoppingInsights.ClosedToOpen.Glic"
_CONFLICTING_HASHES_XML = f"""
<actions>

<action name="{_CONFLICTING_NAME_1}">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Please enter the description of this user action.</description>
</action>

<action name="{_CONFLICTING_NAME_2}">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Please enter the description of this user action.</description>
</action>

</actions>
"""

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

    mock_generate_histogram_list = MagicMock()
    mock_generate_histogram_list.GetActualActionNames.return_value = {
        'Test.Action.Removed'
    }

    with patch.object(PRESUBMIT,
                      'generate_histogram_list',
                      mock_generate_histogram_list), \
        patch.object(PRESUBMIT,
                     'print_action_names',
                     mock_print_action_names):
      errors = PRESUBMIT.CheckRemovedSegmentationUserActions(
          mock_input_api, mock_output_api)

    self.assertEqual(len(errors), 1)
    # Check that PresubmitError was called with the correct message
    mock_output_api.PresubmitError.assert_called_once()
    self.assertIn(
        'The following user actions are used by segmentation platform',
        mock_output_api.PresubmitError.call_args[0][0])

  def testDetectsHashConflict(self):
    mock_input_api = MagicMock()
    mock_output_api = MagicMock()

    mock_input_api.PresubmitLocalPath.return_value = os.path.dirname(__file__)

    with tempfile.NamedTemporaryFile(mode='w+', encoding='utf-8',
                                     delete=False) as temp_file:
      try:
        temp_file.write(_CONFLICTING_HASHES_XML)
        temp_file.close()

        errors = PRESUBMIT._CheckForHashConflicts(temp_file.name,
                                                  mock_input_api,
                                                  mock_output_api)

        self.assertEqual(len(errors), 1)
        mock_output_api.PresubmitError.assert_called_once()
        message = mock_output_api.PresubmitError.call_args[0][0]
        self.assertIn('conflict', message)
        self.assertIn(_CONFLICTING_NAME_1, message)
        self.assertIn(_CONFLICTING_NAME_2, message)
      finally:
        os.remove(temp_file.name)


if __name__ == '__main__':
  unittest.main()
