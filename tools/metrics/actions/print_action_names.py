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
import io

sys.path.append(os.path.dirname(__file__))
import extract_actions


def get_names(xml_content):
  """Returns all action names from an xml string.

  Args:
    xml_content: A string containing actions.xml definitions.
  Returns:
    The set of action names.
  """
  if not xml_content:
    return set()
  actions_dict, _, _ = extract_actions.ParseActionFile(xml_content)
  return set(actions_dict.keys())


def _get_actions_xml_path():
  return os.path.join(os.path.dirname(__file__), 'actions.xml')


def get_action_diff(revision):
  """Returns the added / removed action names relative to git revision

  Args:
    revision: A git revision as described in
      https://git-scm.com/docs/gitrevisions
  Returns:
    A tuple of (added names, removed names), where each entry is sorted in
    ascending order.
  """
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

  current_action_names = get_names(current_content)
  prev_action_names = get_names(prev_content)

  added_names = sorted(list(current_action_names - prev_action_names))
  removed_names = sorted(list(prev_action_names - current_action_names))
  return (added_names, removed_names)


def _print_diff_names(revision):
  added_names, removed_names = get_action_diff(revision)
  print("%d actions added:" % len(added_names))
  for name in added_names:
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
      name_set = get_names(f.read())
    for name in sorted(list(name_set)):
      print(name)


if __name__ == '__main__':
  main(sys.argv)
