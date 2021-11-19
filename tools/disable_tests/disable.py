#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script automatically disables tests, given an ID and a set of
configurations on which it should be disabled. See the README for more details.
"""

import argparse
import os
import sys
import subprocess
import traceback
from typing import List, Optional
import urllib.parse

import conditions
import errors
import expectations
import gtest
import resultdb

SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def main(argv: List[str]) -> int:
  valid_conds = ' '.join(
      sorted(f'\t{term.name}' for term in conditions.TERMINALS))

  parser = argparse.ArgumentParser(
      description='Disables tests.',
      epilog=f"Valid conditions are:\n{valid_conds}")

  parser.add_argument('test_id',
                      type=str,
                      help='the test to disable. For example: ' +
                      'ninja://chrome/test:browser_tests/Suite.Name. You can ' +
                      'also just pass Suite.Name, and the tool will search ' +
                      'for a test with a matching ID')
  parser.add_argument('conditions',
                      type=str,
                      nargs='*',
                      help="the conditions under which to disable the test. " +
                      "Each entry consists of any number of conditions joined" +
                      " with '&', specifying the conjunction of these values." +
                      " All entries will be 'OR'ed together, along with any " +
                      "existing conditions from the file.")
  parser.add_argument('-c',
                      '--cache',
                      action='store_true',
                      help='cache ResultDB rpc results, useful for testing')

  args = parser.parse_args(argv[1:])

  if args.cache:
    resultdb.CANNED_RESPONSE_FILE = os.path.join(os.path.dirname(__file__),
                                                 '.canned_responses.json')

  try:
    disable_test(args.test_id, args.conditions)
    return 0
  except errors.UserError as e:
    print(e, file=sys.stderr)
    return 1
  except errors.InternalError as e:
    trace = traceback.format_exc()
    print(f"Internal error: {e}", file=sys.stderr)
    print('Please file a bug using the following link:', file=sys.stderr)
    print(generate_bug_link(args, trace), file=sys.stderr)
    return 1
  except Exception:
    trace = traceback.format_exc()
    print(f'Error: unhandled exception at top-level\n{trace}', file=sys.stderr)
    print('Please file a bug using the following link:', file=sys.stderr)
    print(generate_bug_link(args, trace), file=sys.stderr)
    return 1


# TODO: Extra command line flags for:
#   * Opening the right file at the right line, for when you want to do
#     something manually. Use $EDITOR.
#   * Adding a comment / message accompanying the disablement.
#   * Printing out all valid configs.
#   * Overwrite the existing state rather than adding to it. Probably leave this
#     until it's requested.
def disable_test(test_id: str, cond_strs: List[str]):
  conds = conditions.parse(cond_strs)

  #  If the given ID starts with "ninja:", then it's a full test ID. If not,
  #  assume it's a test name, and transform it into a query that will match the
  #  full ID.
  if not test_id.startswith('ninja:'):
    test_id = f'ninja://.*/{extract_name_and_suite(test_id)}(/.*)?'

  test_name, filename = resultdb.get_test_metadata(test_id)
  test_name = extract_name_and_suite(test_name)

  # Paths returned from ResultDB look like //foo/bar, where // refers to the
  # root of the chromium/src repo.
  full_path = os.path.join(SRC_ROOT, filename.lstrip('/'))
  _, extension = os.path.splitext(full_path)
  extension = extension.lstrip('.')

  if extension == 'html':
    full_path = expectations.search_for_expectations(full_path, test_name)

  try:
    with open(full_path, 'r') as f:
      source_file = f.read()
  except FileNotFoundError as e:
    raise errors.UserError(
        f"Couldn't open file {filename}. Either this test has moved file very" +
        "recently, or your checkout isn't up-to-date.") from e

  if extension == 'cc':
    disabler = gtest.disabler
  elif extension == 'html':
    disabler = expectations.disabler
  else:
    raise errors.UserError(
        f"Don't know how to disable tests for this file format ({extension})")

  new_content = disabler(test_name, source_file, conds)
  with open(full_path, 'w') as f:
    f.write(new_content)


def extract_name_and_suite(test_name: str) -> str:
  # Web tests just use the filename as the test name, so don't mess with it.
  if test_name.endswith('.html'):
    return test_name

  # GTest Test names always have a suite name and test name, separated by '.'s.
  # They may also have extra slash-separated parts on the beginning and the end,
  # for parameterised tests.
  for part in test_name.split('/'):
    if '.' in part:
      return part

  raise errors.UserError(f"Couldn't parse test name: {test_name}")


def get_current_commit_hash() -> Optional[str]:
  proc = subprocess.run(['git', 'rev-parse', 'HEAD'],
                        check=False,
                        capture_output=True,
                        text=True)
  if proc.returncode != 0:
    return None

  return proc.stdout.strip()


# TODO: Ideally we'd also capture all RPC results so we can 100% reproduce it.
def generate_bug_link(args: argparse.Namespace, trace: str) -> str:
  # Strip path prefixes to avoid leaking info about the user.
  trace = trace.replace(SRC_ROOT, '/')

  args_list = '\n'.join(f'{k} = {v}' for k, v in args.__dict__.items())

  summary = f'Test disabler failed for {args.test_id}'
  description = f'''
<Please describe the problem here>

========== Debug info ==========

Exception:
{trace}
Args:
{args_list}'''

  if (git_hash := get_current_commit_hash()) is not None:
    description += f'''

Checked out chromium/src revision:
{git_hash}
'''

  params = urllib.parse.urlencode(
      dict(
          labels='Type-Bug,Pri-2',
          # TODO: Consider separating the tool out into its own component. Or
          # perhaps just adding a label like 'Test-Disabling-Tool'.
          components='Infra>Sheriffing>SheriffOMatic',
          summary=summary,
          description=description,
      ))

  return urllib.parse.urlunsplit(
      ('https', 'bugs.chromium.org', '/p/chromium/issues/entry', params, ''))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
