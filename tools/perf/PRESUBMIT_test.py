# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for tools/perf/PRESUBMIT.py."""

import ntpath
import os
import re
import sys
import unittest
from unittest import mock

_PERF_DIR = os.path.abspath(os.path.dirname(__file__))
_SRC_DIR = os.path.abspath(os.path.join(_PERF_DIR, '..', '..'))
if _PERF_DIR not in sys.path:
  sys.path.insert(0, _PERF_DIR)
if _SRC_DIR not in sys.path:
  sys.path.append(_SRC_DIR)

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

import PRESUBMIT


class ValidationScriptTest(unittest.TestCase):
  def setUp(self):
    self.input_api = MockInputApi()
    self.input_api.presubmit_local_path = _PERF_DIR
    self.output_api = MockOutputApi()

  def testGetValidationCommandsDefault(self):
    commands = PRESUBMIT._GetValidationCommands(
      self.input_api,
      self.output_api,
      'dummy_script',
      extra_args=['--arg1', '--arg2'],
      block_on_failure=True,
    )
    self.assertEqual(len(commands), 1)
    cmd = commands[0]
    self.assertEqual(cmd.name, 'Script dummy_script')
    self.assertEqual(cmd.message, self.output_api.PresubmitError)
    expected_script = os.path.join(_PERF_DIR, 'dummy_script')
    self.assertIn(expected_script, cmd.cmd)
    self.assertIn('--arg1', cmd.cmd)
    self.assertIn('--arg2', cmd.cmd)

  def testGetValidationCommandsPromptWarning(self):
    commands = PRESUBMIT._GetValidationCommands(
      self.input_api,
      self.output_api,
      'dummy_script',
      block_on_failure=False,
    )
    self.assertEqual(len(commands), 1)
    self.assertEqual(
      commands[0].message, self.output_api.PresubmitPromptWarning
    )

  def testGetValidationCommandsSplitsArgs(self):
    limit = 50 if self.input_api.is_windows else 1000
    extra_args = [f'file_{i}' for i in range(limit + 10)]
    commands = PRESUBMIT._GetValidationCommands(
      self.input_api,
      self.output_api,
      'dummy_script',
      extra_args=extra_args,
    )
    self.assertEqual(len(commands), 2)
    self.assertEqual(len(commands[0].cmd), 2 + limit)
    self.assertEqual(len(commands[1].cmd), 2 + 10)

  def testCommonChecksAggregatesCommands(self):
    commands_executed = []

    def mock_run_tests(commands):
      commands_executed.extend(commands)
      return []

    self.input_api.RunTests = mock_run_tests
    results = PRESUBMIT._CommonChecks(
      self.input_api, self.output_api, block_on_failure=False
    )
    self.assertEqual(results, [])
    self.assertEqual(len(commands_executed), 3)
    cmd_names = [c.name for c in commands_executed]
    self.assertIn('Script generate_perf_data', cmd_names)
    self.assertIn('Script validate_perf_json_config', cmd_names)
    self.assertIn('Script generate_perf_sharding.py', cmd_names)
    for c in commands_executed:
      self.assertEqual(c.message, self.output_api.PresubmitPromptWarning)

  def testCommonChecksBlocksOnCommit(self):
    commands_executed = []

    def mock_run_tests(commands):
      commands_executed.extend(commands)
      return []

    self.input_api.RunTests = mock_run_tests
    PRESUBMIT._CommonChecks(
      self.input_api, self.output_api, block_on_failure=True
    )
    self.assertEqual(len(commands_executed), 3)
    for c in commands_executed:
      self.assertEqual(c.message, self.output_api.PresubmitError)


class MockAffectedFile:
  def __init__(self, path, action='A'):
    self._path = path
    self._action = action

  def Action(self):
    return self._action

  def AbsoluteLocalPath(self):
    return self._path

  def LocalPath(self):
    return self._path


class MockCannedChecks:
  pass


class MockPylintInputApi:
  def __init__(self, affected_files=None, no_diffs=False):
    self.os_path = os.path
    self.re = re
    self.no_diffs = no_diffs
    self.canned_checks = MockCannedChecks()
    self._presubmit_local_path = _PERF_DIR
    self._affected_files = list(affected_files or [])

  def PresubmitLocalPath(self):
    return self._presubmit_local_path

  def AffectedFiles(self, include_deletes=True):
    for f in self._affected_files:
      if not include_deletes and f.Action() == 'D':
        continue
      yield f

  def InitFiles(self, files):
    self._affected_files = list(files)

  def RunTests(self, tests):
    del tests  # Unused.
    return []


class MockPylintOutputApi:
  pass


class PresubmitUnittestsTest(unittest.TestCase):
  def setUp(self):
    self.input_api = MockInputApi()
    self.output_api = MockOutputApi()

  def testCheckPresubmitUnittestsRunsTest(self):
    with mock.patch.object(
      self.input_api.canned_checks,
      'RunUnitTestsInDirectory',
      create=True,
      return_value=[],
    ) as mock_run:
      results = PRESUBMIT.CheckPresubmitUnittests(
        self.input_api, self.output_api
      )
      mock_run.assert_called_once_with(
        self.input_api,
        self.output_api,
        self.input_api.PresubmitLocalPath(),
        [r'^PRESUBMIT_test\.py$'],
      )
      self.assertEqual(results, [])


if __name__ == '__main__':
  unittest.main()
