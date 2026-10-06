# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for tools/perf/PRESUBMIT.py."""

# pylint: disable=bad-indentation

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

# pylint: disable=import-error
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

import PRESUBMIT

# pylint: disable=protected-access


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


class GetPylintFilesToCheckTest(unittest.TestCase):
  def setUp(self):
    self.input_api = MockPylintInputApi()
    self.output_api = MockPylintOutputApi()

  def testNoDiffsReturnsNone(self):
    self.input_api.no_diffs = True
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(self.input_api))

  def testPresubmitModifiedReturnsNone(self):
    presubmit_file = MockAffectedFile(os.path.join(_PERF_DIR, 'PRESUBMIT.py'))
    self.input_api.InitFiles([presubmit_file])
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(self.input_api))

  def testPylintrcModifiedReturnsNone(self):
    pylintrc_file = MockAffectedFile(os.path.join(_PERF_DIR, 'pylintrc'))
    self.input_api.InitFiles([pylintrc_file])
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(self.input_api))

  def testRootPythonFileModifiedReturnsNone(self):
    root_py = MockAffectedFile(os.path.join(_PERF_DIR, 'generate_perf_data.py'))
    self.input_api.InitFiles([root_py])
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(self.input_api))

  def testDeletedPythonFilesReturnsNone(self):
    deleted_py = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'deleted.py'), action='D'
    )
    self.input_api.InitFiles([deleted_py])
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(self.input_api))

  def testDeletedNonPythonFilesReturnsEmpty(self):
    deleted_json = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'deleted.json'), action='D'
    )
    self.input_api.InitFiles([deleted_json])
    self.assertEqual(PRESUBMIT._GetPylintFilesToCheck(self.input_api), [])

  def testNonPythonFilesReturnsEmpty(self):
    json_file = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'shard_maps', 'test.json')
    )
    self.input_api.InitFiles([json_file])
    self.assertEqual(PRESUBMIT._GetPylintFilesToCheck(self.input_api), [])

  def testFilesOutsidePerfReturnsEmpty(self):
    outside_file = MockAffectedFile(os.path.join(_SRC_DIR, 'chrome', 'test.py'))
    self.input_api.InitFiles([outside_file])
    self.assertEqual(PRESUBMIT._GetPylintFilesToCheck(self.input_api), [])

  def testSingleSubdirectoryScoped(self):
    py1 = MockAffectedFile(os.path.join(_PERF_DIR, 'core', 'bot_platforms.py'))
    py2 = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'perf_data_generator.py')
    )
    self.input_api.InitFiles([py1, py2])
    patterns = PRESUBMIT._GetPylintFilesToCheck(self.input_api)
    self.assertEqual(patterns, [r'core(?:/|\\).*\.py$'])

  def testMultipleSubdirectoriesScoped(self):
    py1 = MockAffectedFile(
      os.path.join(_PERF_DIR, 'benchmarks', 'benchmark.py')
    )
    py2 = MockAffectedFile(os.path.join(_PERF_DIR, 'core', 'bot_platforms.py'))
    self.input_api.InitFiles([py1, py2])
    patterns = PRESUBMIT._GetPylintFilesToCheck(self.input_api)
    self.assertEqual(
      patterns,
      [r'benchmarks(?:/|\\).*\.py$', r'core(?:/|\\).*\.py$'],
    )

  def testNestedSubdirectoryScopedToFirstLevel(self):
    py_file = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'shard_maps', 'test.py')
    )
    self.input_api.InitFiles([py_file])
    patterns = PRESUBMIT._GetPylintFilesToCheck(self.input_api)
    self.assertEqual(patterns, [r'core(?:/|\\).*\.py$'])

  def testWindowsPathSeparatorsAndCasing(self):
    perf_dir_win = ntpath.normpath('C:/src/chromium/src/tools/perf')
    file_path_win = ntpath.normpath(
      'C:/src/chromium/src/tools/perf/Core/bot_platforms.py'
    )
    mock_file = MockAffectedFile(file_path_win)
    input_api = MockPylintInputApi()
    input_api.os_path = ntpath
    input_api._presubmit_local_path = perf_dir_win
    input_api.InitFiles([mock_file])
    patterns = PRESUBMIT._GetPylintFilesToCheck(input_api)
    self.assertEqual(patterns, [r'Core(?:/|\\).*\.py$'])

  def testCheckPyLintReturnsEmptyWhenNoPythonFiles(self):
    json_file = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'shard_maps', 'test.json')
    )
    self.input_api.InitFiles([json_file])
    with mock.patch.object(
      self.input_api.canned_checks, 'GetPylint', create=True
    ) as mock_get_pylint:
      results = PRESUBMIT.CheckPyLint(self.input_api, self.output_api)
      mock_get_pylint.assert_not_called()
      self.assertEqual(results, [])

  def testCheckPyLintPassesFilesToCheck(self):
    py_file = MockAffectedFile(
      os.path.join(_PERF_DIR, 'core', 'bot_platforms.py')
    )
    self.input_api.InitFiles([py_file])
    with mock.patch.object(
      self.input_api.canned_checks, 'GetPylint', create=True, return_value=[]
    ) as mock_get_pylint:
      with mock.patch.object(
        self.input_api, 'RunTests', return_value=[]
      ) as mock_run_tests:
        results = PRESUBMIT.CheckPyLint(self.input_api, self.output_api)
        mock_get_pylint.assert_called_once()
        _, kwargs = mock_get_pylint.call_args
        self.assertEqual(kwargs['files_to_check'], [r'core(?:/|\\).*\.py$'])
        mock_run_tests.assert_called_once()
        self.assertEqual(results, [])

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
