# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for actions.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import copy
import os
import sys


def CheckChange(input_api, output_api):
  """Checks that actions.xml is up to date and pretty-printed."""
  for f in input_api.AffectedFiles():
    p = f.AbsoluteLocalPath()
    if (input_api.basename(p) == 'actions.xml'
        and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()):
      cwd = input_api.os_path.dirname(p)
      exit_code = input_api.subprocess.call(
          [input_api.python3_executable, 'extract_actions.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                'tools/metrics/actions/actions.xml is not up to date or is not '
                'formatted correctly; run tools/metrics/actions/'
                'extract_actions.py to fix')
        ]
  return []


def CheckRemovedSegmentationUserActions(input_api, output_api):
  """Checks if any user action is removed from actions.xml."""
  actions_xml_path = os.path.join('tools', 'metrics', 'actions', 'actions.xml')

  # Only run check if actions.xml is changed.
  if not any(
      os.path.normpath(f.LocalPath()) == actions_xml_path
      for f in input_api.AffectedFiles(include_deletes=True)):
    return []

  # Add actions dir to sys.path to import print_action_names
  actions_dir = input_api.PresubmitLocalPath()
  sys.path.append(actions_dir)

  removed_actions = []
  try:
    import print_action_names
    # get_action_diff compares the working directory with HEAD~, which is
    # what we wantfor presubmit.
    _added_names, removed_names = print_action_names.get_action_diff('HEAD~')
    removed_actions = removed_names
  except Exception as e:
    return [output_api.PresubmitError(f'Error getting user action diff: {e}')]

  tools_dir = input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..',
                                     '..', 'components',
                                     'segmentation_platform', 'tools')
  sys.path.append(tools_dir)

  try:
    import generate_histogram_list
  except ImportError:
    return [
        output_api.PresubmitError(
            'Could not import generate_histogram_list.py. Make sure the path '
            'is correct.')
    ]

  # Load the list of all actions required by segmentation models.
  segmentation_actions = generate_histogram_list.GetActualActionNames()

  if not segmentation_actions:
    # If the file is empty or doesn't exist, there's nothing to check.
    return []

  removed_segmentation_actions = set(removed_actions).intersection(
      segmentation_actions)

  if removed_segmentation_actions:
    error_message = (
        'The following user actions are used by segmentation platform and '
        'should not be removed without a migration plan. Please reach out '
        'to chrome-segmentation-platform@google.com for questions.')
    return [
        output_api.PresubmitError(error_message,
                                  items=sorted(list(removed_actions)))
    ]

  return []


def CheckChangeOnUpload(input_api, output_api):
  # Store sys path to avoid its pollution by the PRESUBMIT
  original_sys_path = copy.deepcopy(sys.path)
  try:
    results = CheckChange(input_api, output_api)
  finally:
    sys.path = original_sys_path
  return results


def CheckChangeOnCommit(input_api, output_api):
  # Store sys path to avoid its pollution by the PRESUBMIT
  original_sys_path = copy.deepcopy(sys.path)
  try:
    results = CheckChange(input_api, output_api)
  finally:
    sys.path = original_sys_path
  return results
