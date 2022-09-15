# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code specific to disabling tests using test expectations files"""

import collections
import os
import sys
from typing import Optional, List

import conditions
from conditions import Condition
import errors

sys.path.append(
    os.path.join(os.path.dirname(__file__), '..', '..', 'third_party',
                 'catapult', 'third_party', 'typ'))
from typ.expectations_parser import (  # type: ignore
    Expectation, TaggedTestListParser)
sys.path.pop()


def search_for_expectations(filename: str, test_name: str) -> str:
  # Web test have "virtual test suites", where the same set of tests is run with
  # different parameters. These are specified by "VirtualTestSuites" files, but
  # generally the way it works is that a test a/b/c.html will have a virtual
  # test in virtual/foo/a/b/c.html. So we handle this by stripping parts of the
  # path off until the filename and test name match.
  while not filename.endswith(test_name) and '/' in test_name:
    test_name = test_name[test_name.index('/') + 1:]

  # TODO: Is this ever not the case? If so we might need to just keep searching
  # upwards, directory by directory until we find a test expectations file
  # referencing this test.
  assert filename.endswith(test_name)

  expectations_dir = filename[:-len(test_name)]
  # TODO: I think ASan and some other conditions are handled via different
  # files.
  expectations_path = os.path.join(expectations_dir, 'TestExpectations')
  if os.path.exists(expectations_path):
    return expectations_path

  raise errors.InternalError("Couldn't find TestExpectations file for test " +
                             f"{test_name} " +
                             f"(expected to find it at {expectations_path})")


def disabler(full_test_name: str, source_file: str, new_cond: Condition,
             message: Optional[str]) -> str:
  comment = None
  if message:
    comment = f"# {message}"

  existing_expectation: Optional[Expectation] = None
  condition = conditions.NEVER
  for expectation in TaggedTestListParser(source_file).expectations:
    if expectation.test == full_test_name:
      existing_expectation = expectation
      if set(expectation.results) & {'SKIP', 'FAIL'}:
        tags = set(expectation.tags)
        if not tags:
          condition = conditions.ALWAYS

      break

  merged = conditions.merge(condition, new_cond)

  if existing_expectation is None:
    ex = Expectation(test=full_test_name,
                     results=['SKIP'],
                     tags=condition_to_tags(merged))

    while not source_file.endswith('\n\n'):
      source_file += '\n'

    if comment:
      source_file += f"{comment}\n"

    source_file += ex.to_string()
    return source_file

  new_expectation = Expectation(
      reason=existing_expectation.reason,
      test=existing_expectation.test,
      trailing_comments=existing_expectation.trailing_comments,
      results=['SKIP'],
      tags=condition_to_tags(merged),
  )

  lines = source_file.split('\n')
  # Minus 1 as 'lineno' is 1-based.
  lines[existing_expectation.lineno - 1] = new_expectation.to_string()

  if comment:
    lines.insert(existing_expectation.lineno - 1, comment)

  return '\n'.join(lines)


ExpectationsInfo = collections.namedtuple('ExpectationsInfo', 'tag')

for t_name, t_tag in [
    ('android', 'Android'),
    ('fuchsia', 'Fuchsia'),
    ('linux', 'Linux'),
    ('mac', 'Mac'),
    ('win', 'Win'),
]:
  conditions.get_term(t_name).expectations_info = ExpectationsInfo(tag=t_tag)


def condition_to_tags(cond: Condition) -> List[str]:
  if cond is conditions.ALWAYS:
    return []

  assert not isinstance(cond, conditions.BaseCondition)

  if isinstance(cond, conditions.Terminal):
    return [conditions.get_term(cond.name).expectations_info.tag]

  op, args = cond
  if op == 'or':
    return [tag for arg in args for tag in condition_to_tags(arg)]

  raise errors.InternalError(
      f"Unable to express condition in test expectations format: {cond}")
