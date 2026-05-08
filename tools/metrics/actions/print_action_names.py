#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints all user action names."""

from __future__ import print_function

import argparse
import os
import subprocess
import sys

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.actions.action_utils as action_utils
import chromium_src.tools.metrics.common.path_util as path_util


def _get_actions(xml_content):
  """Returns all action from an xml string.

  Args:
    xml_content: A string containing actions.xml definitions.
  Returns:
    The dictionary containing mapping from action name to action object.
  """
  if not xml_content:
    return {}
  actions_dict, _, _ = action_utils.ParseActionFile(xml_content)
  return actions_dict


def get_modified_action_names(current_actions_dict, prev_actions_dict,
                              added_names):
  """Returns all modified action names from two xml strings.

  Args:
    current_content: A string containing actions.xml definitions.
    prev_content: A string containing actions.xml definitions.
  Returns:
    The set of action names.
  """
  modified_names = []
  for name, action in current_actions_dict.items():
    if name in added_names:
      continue
    if action != prev_actions_dict[name]:
      modified_names.append(name)
  return modified_names


def _get_actions_xml_path() -> str:
  return str(path_util.METRICS_TOOLS_PATH / 'actions' / 'actions.xml')


def get_action_diff(prev_content, current_content):
  """Returns the added, modified, and removed action names.

  This diff is relative to the old version of the file.

  Args:
    prev_content: A string containing actions.xml definitions for the previous
      revision.
    current_content: A string containing actions.xml definitions for the
      current revision.
  Returns:
    A tuple of (added names, removed names, modified names), where each entry is
    a list of strings sorted in ascending order.
  """

  current_actions_dict = _get_actions(current_content)
  prev_actions_dict = _get_actions(prev_content)

  added_names = sorted(current_actions_dict.keys() - prev_actions_dict.keys())
  removed_names = sorted(prev_actions_dict.keys() - current_actions_dict.keys())

  modified_names = get_modified_action_names(current_actions_dict,
                                             prev_actions_dict, added_names)

  return (added_names, removed_names, modified_names)


def _print_diff_names(revision):
  """Prints the added / removed action names relative to provided revision."""
  actions_xml_path = _get_actions_xml_path()
  actions_xml_path_relative = os.path.join('tools', 'metrics', 'actions',
                                           'actions.xml')

  try:
    prev_content = subprocess.check_output(
        ['git', 'show',
         f'{revision}:{actions_xml_path_relative}']).decode('utf-8')
  except subprocess.CalledProcessError:
    # Path might not exist in the provided revision.
    prev_content = ''

  with open(actions_xml_path, 'r', encoding='utf-8') as f:
    current_content = f.read()

  added_names, removed_names, modified_names = get_action_diff(
      prev_content, current_content)
  print("%d actions added:" % len(added_names))
  for name in added_names:
    print(name)

  print("%d actions modified:" % len(modified_names))
  for name in modified_names:
    print(name)

  print("%d actions removed:" % len(removed_names))
  for name in removed_names:
    print(name)


def main(argv):
  parser = argparse.ArgumentParser(description='Print user action names.')
  parser.add_argument('--diff',
                      type=str,
                      help='Git revision to diff against (e.g. HEAD~)')
  args = parser.parse_args(argv[1:])
  if args.diff is not None:
    _print_diff_names(args.diff)
  else:
    with open(_get_actions_xml_path(), 'r', encoding='utf-8') as f:
      actions_dict = _get_actions(f.read())
    for name in sorted(actions_dict.keys()):
      print(name)


if __name__ == '__main__':
  main(sys.argv)
