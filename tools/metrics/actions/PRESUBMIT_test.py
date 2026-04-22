# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest.mock import MagicMock, patch, call
import sys
import os
import tempfile

import setup_modules  # pylint: disable=unused-import

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

_OWNERS_OLD_XML = """
<actions>
<action name="ModifiedAction.ToInvalid">
  <owner>valid@chromium.org</owner>
  <description>Was valid.</description>
</action>
<action name="OldAction.Invalid">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Was invalid.</description>
</action>
<action name="OldAction.Invalid2">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Remains invalid.</description>
</action>
<action name="OldAction.Valid">
  <owner>owner@chromium.org</owner>
  <description>Valid owner.</description>
</action>
</actions>"""

_OWNERS_NEW_XML = """<actions>
<action name="ModifiedAction.ToInvalid">
  <owner>Please list the metric's owners.</owner>
  <description>Was valid, now invalid.</description>
</action>
<action name="NewAction.Invalid">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>New invalid.</description>
</action>
<action name="NewAction.Valid">
  <owner>new@chromium.org</owner>
  <description>New valid.</description>
</action>
<action name="OldAction.Invalid">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Modified description, still invalid.</description>
</action>
<action name="OldAction.Invalid2">
  <owner>Please list the metric's owners. Add more owner tags as needed.</owner>
  <description>Remains invalid.</description>
</action>
<action name="OldAction.Valid">
  <owner>owner@chromium.org</owner>
  <description>Still valid.</description>
</action>
</actions>"""


class PresubmitTest(unittest.TestCase):

  def testCheckRemovedSegmentationUserActions(self):
    mock_input_api = MagicMock()
    mock_output_api = MagicMock()

    # Create a mock error object to be returned by PresubmitError
    mock_error = MagicMock()
    mock_output_api.PresubmitError.return_value = mock_error

    mock_file = MagicMock()
    mock_file.LocalPath.return_value = os.path.join('tools', 'metrics',
                                                    'actions', 'actions.xml')
    mock_input_api.AffectedFiles.return_value = [mock_file]

    # Mock the imported module
    mock_print_action_names = MagicMock()
    mock_print_action_names.get_action_diff.return_value = ([], [
        'Test.Action.Removed'
    ], [])

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

  def testCheckActionOwners(self):
    mock_input_api = MagicMock()
    mock_output_api = MagicMock()
    mock_error = MagicMock()
    mock_output_api.PresubmitError.return_value = mock_error
    mock_file = MagicMock()
    # Path used for matching in PRESUBMIT.py
    mock_file.LocalPath.return_value = os.path.join('tools', 'metrics',
                                                    'actions', 'actions.xml')
    mock_file.OldContents.return_value = _OWNERS_OLD_XML.splitlines()
    mock_file.NewContents.return_value = _OWNERS_NEW_XML.splitlines()

    mock_input_api.AffectedFiles.return_value = [mock_file]
    mock_input_api.os_path = os

    errors = PRESUBMIT._CheckActionOwners(mock_input_api, mock_output_api)

    self.assertEqual(len(errors), 3,
                     f"Expected 3 errors, got {len(errors)}. Errors: {errors}")
    self.assertEqual(mock_output_api.PresubmitError.call_count, 3)

    expected_calls = [
        call('Action NewAction.Invalid has no valid owners. If this is an '
             'unrelated change, you can ignore this error and bypass the '
             'presubmit check.'),
        call('Action ModifiedAction.ToInvalid has no valid owners. If this is '
             'an unrelated change, you can ignore this error and bypass the '
             'presubmit check.'),
        call('Action OldAction.Invalid has no valid owners. If this is an '
             'unrelated change, you can ignore this error and bypass the '
             'presubmit check.')
    ]
    mock_output_api.PresubmitError.assert_has_calls(expected_calls,
                                                    any_order=True)

if __name__ == '__main__':
  unittest.main()
