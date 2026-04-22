# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for actions.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import hashlib
import os
import struct
from typing import Any, Sequence
import xml.dom.minidom

import sys

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.
# TODO(crbug.com/488351821): Consider using subprocesses to run actual
#                            test as recommended by presubmit docs:
# https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
sys.path.append('.')
import setup_modules  # pylint: disable=unused-import

sys.path.remove('.')

import chromium_src.components.segmentation_platform.tools.generate_histogram_list as generate_histogram_list
import chromium_src.tools.metrics.actions.action_utils as action_utils
import chromium_src.tools.metrics.actions.print_action_names as print_action_names

ACTIONS_XML_PATH = os.path.join('tools', 'metrics', 'actions', 'actions.xml')


# TODO: Unify with hashing library to avoid copying it directly.
def _HashHistogramNameToInt(t: str) -> int:
  """Computes the hash of a Chrome histogram name.

  The histogram ID is derived from the first 8 bytes of the md5 hash of the
  histogram's name. Those 64 bits are interpreted as an integer (big-endian
  unsigned long long). See HashMetricName here:
  https://source.chromium.org/chromium/chromium/src/+/main:base/metrics/metrics_hashes.cc

  Args:
    t: The string to hash (a histogram name).

  Returns:
    Histogram hash as an integer.
  """
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(t.encode('utf-8')).digest()[:8])[0]


# TODO: Unify with hashing library to avoid copying it directly.
def _HashUserActionToInt(useraction: str) -> int:
  """Computes the bucket index of a user action in UserActions.Counts.

  The bucket ID is the positive int32 version of the histogram hash for
  the user action name.

  Args:
    useraction: Name of useraction (case sensitive)

  Returns:
    Bucket index (min) of useraction in UserActions.Counts histogram.
  """
  # >Q: 8 bytes, big endian.
  return _HashHistogramNameToInt(useraction) & 0x7FFFFFFF


def _IsActionPath(input_api: Any, p: str) -> bool:
  return (input_api.basename(p) == 'actions.xml'
          and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath())


def _ActionsXmlPath(input_api: Any) -> str | None:
  for f in input_api.AffectedFiles():
    p = f.AbsoluteLocalPath()
    if _IsActionPath(input_api, p):
      return p
  return None


def _CheckPrettyPrint(actions_xml_path: str, input_api: Any,
                      output_api: Any) -> Sequence[Any]:
  """Checks that actions.xml is up to date and pretty-printed."""

  cwd = input_api.os_path.dirname(actions_xml_path)
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


def _CheckForHashConflicts(actions_xml_path: str, input_api: Any,
                           output_api: Any) -> Sequence[Any]:
  """Generates hashes of all the actions and validates there are no repeats

  This generates all actions variants and makes sure that among all the
  variants there are no conflicts in a hashes of their name. This also
  includes a hash that is used for a bucket in action histogram at the end
  of the process.
  """
  with open(actions_xml_path, 'r') as f:
    data = f.read()

  actions_dict, _, _ = action_utils.ParseActionFile(data)
  expanded_actions = action_utils.CreateActionsFromVariants(actions_dict)

  name_by_hash: dict[int, str] = {}
  errors = []

  for action_name in expanded_actions.keys():
    hash = _HashUserActionToInt(action_name)
    if hash in name_by_hash:
      errors.append(
          output_api.PresubmitError(
              f'Hash conflict! Multiple actions share the same hash: {hash},'
              f' affected actions: {name_by_hash[hash]}, {action_name}.'
              f' Please rename your new action!'))
    name_by_hash[hash] = action_name

  return errors


def _CheckActionOwners(input_api: Any, output_api: Any) -> list[Any]:
  """Validates that all new or modified actions have valid owners.

  Args:
    input_api: The input API for the presubmit.
    output_api: The output API for the presubmit.

  Returns:
    A list of presubmit errors.
  """
  results: list[Any] = []

  # Only run check if actions.xml is changed.
  actions_xml_file = [
      f for f in input_api.AffectedFiles()
      if os.path.normpath(f.LocalPath()) == ACTIONS_XML_PATH
  ]

  if not actions_xml_file:
    return results
  assert len(actions_xml_file) == 1
  f = actions_xml_file[0]
  added_names, _, modified_names = (print_action_names.get_action_diff(
      '\n'.join(f.OldContents()), '\n'.join(f.NewContents())))

  if not added_names and not modified_names:
    return results

  data = '\n'.join(f.NewContents())
  actions_dict, _, _ = action_utils.ParseActionFile(data)

  for name in added_names + modified_names:
    action = actions_dict[name]
    _, has_valid_owner = action_utils.ExtractOwners(action.owners or [])

    if not has_valid_owner:
      results.append(
          output_api.PresubmitError(
              f'Action {name} has no valid owners. If this is an unrelated '
              'change, you can ignore this error and bypass the presubmit '
              'check.'))

  return results


def CheckChange(input_api, output_api):
  actions_xml_path = _ActionsXmlPath(input_api)
  if not actions_xml_path:
    return []

  problems = []
  problems.extend(_CheckPrettyPrint(actions_xml_path, input_api, output_api))
  problems.extend(
      _CheckForHashConflicts(actions_xml_path, input_api, output_api))
  problems.extend(CheckRemovedSegmentationUserActions(input_api, output_api))
  problems.extend(_CheckActionOwners(input_api, output_api))
  return problems


def CheckRemovedSegmentationUserActions(input_api, output_api):
  """Checks if any user action is removed from actions.xml."""
  # Only run check if actions.xml is changed.
  actions_xml_file = [
      f for f in input_api.AffectedFiles(include_deletes=True)
      if os.path.normpath(f.LocalPath()) == ACTIONS_XML_PATH
  ]
  if not actions_xml_file:
    return []
  assert len(actions_xml_file) == 1
  f = actions_xml_file[0]
  try:
    # get_action_diff compares new and old contents of the file and returns
    # the added and removed action names.
    _, removed_names, _ = print_action_names.get_action_diff(
        '\n'.join(f.OldContents()), '\n'.join(f.NewContents()))
    removed_actions = removed_names
  except Exception as e:
    return [output_api.PresubmitError(f'Error getting user action diff: {e}')]
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
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
