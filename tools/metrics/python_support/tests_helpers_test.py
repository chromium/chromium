# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
from pathlib import Path
from unittest.mock import patch, mock_open

import setup_modules
import chromium_src.tools.metrics.python_support.tests_helpers as tests_helpers
import chromium_src.tools.metrics.python_support.script_checker as script_checker


class TestableScriptListTest(unittest.TestCase):

  def testAllTestableScriptsCanBeRun(self):
    script_issues = script_checker.check_scripts(
        tests_helpers._TESTABLE_SCRIPTS, cwd=tests_helpers._CHROMIUM_SRC_DIR)
    for issue in script_issues:
      print(issue.error_message())
    self.assertEqual(len(script_issues), 0)


class TestableScriptUtilTest(unittest.TestCase):

  def testPythonTestableScriptCreation(self):
    script_path = Path("some").joinpath("path").joinpath("script.py")
    flags = ["--flag1", "value1"]

    script = tests_helpers.TestableScript.CreatePythonScript(script_path, flags)

    self.assertEqual(script.identifiable_name, str(script_path))
    self.assertEqual(script.file_path, script_path)
    self.assertEqual(
        script.cmd,
        ["vpython3", str(script_path), "--flag1", "value1"])


class TestScanningTest(unittest.TestCase):

  def testTestFileDetection(self):
    self.assertTrue(tests_helpers._is_test_file("foo_test.py"))
    self.assertTrue(tests_helpers._is_test_file("bar_tests.py"))
    self.assertFalse(tests_helpers._is_test_file("foo.py"))
    self.assertFalse(tests_helpers._is_test_file("test_foo.txt"))

  @patch('os.walk')
  def testFindAllTestsWalksRecursively(self, mock_walk):
    # Mock os.walk to return a fake directory tree
    mock_walk.return_value = [
        (os.path.join('fake',
                      'dir1'), ['subdir'], ['a_test.py', 'different_test.py']),
        (os.path.join('fake', 'dir1', 'subdir'), [], ['b_tests.py']),
    ]

    # Patch the module's directories to scan so it uses our fake ones
    with patch.object(tests_helpers, 'TEST_DIRECTORIES_RELATIVE_TO_SRC',
                      [os.path.join('fake', 'dir1')]):
      tests = list(tests_helpers.find_all_tests())

      expected_tests = [
          os.path.join('fake', 'dir1', 'a_test.py'),
          os.path.join('fake', 'dir1', 'different_test.py'),
          os.path.join('fake', 'dir1', 'subdir', 'b_tests.py')
      ]
      self.assertEqual(sorted(tests), sorted(expected_tests))


class BuildGnValidationTest(unittest.TestCase):

  @patch('pathlib.Path.is_dir')
  @patch('pathlib.Path.is_file')
  @patch(
      'chromium_src.tools.metrics.python_support.tests_helpers.find_all_tests')
  def testCheckBuildGnSourcesValidation(self, mock_find_tests, mock_is_file,
                                        mock_is_dir):
    mock_is_dir.return_value = True
    mock_is_file.return_value = True

    mock_find_tests.return_value = [
        os.path.join(tests_helpers._CHROMIUM_SRC_DIR, 'tools', 'metrics',
                     'test1_test.py'),
        os.path.join(tests_helpers._CHROMIUM_SRC_DIR, 'tools', 'metrics',
                     'test2_test.py'),
    ]

    fake_gn_content = """
group("metrics_python_tests") {
    data = [
        "//tools/metrics/test1_test.py",
    ]
}
        """

    with patch("builtins.open", mock_open(read_data=fake_gn_content)):
      missing_files = tests_helpers.validate_gn_sources("metrics_python_tests")

      # test2_test.py should be flagged as missing
      self.assertEqual(len(missing_files), 1)
      self.assertIn(os.path.join('tools', 'metrics', 'test2_test.py'),
                    list(missing_files)[0])

  @patch('pathlib.Path.is_dir')
  @patch('pathlib.Path.is_file')
  def testMissingValidatedGroupInGn(self, mock_is_file, mock_is_dir):
    """Test validate_gn_sources raises ValueError if group is missing."""
    mock_is_dir.return_value = True
    mock_is_file.return_value = True

    fake_gn_content = """
group("wrong_group") {
    data = []
}
        """

    with patch("builtins.open", mock_open(read_data=fake_gn_content)):
      with self.assertRaisesRegex(ValueError, "Could not find group"):
        tests_helpers.validate_gn_sources("metrics_python_tests")


class AffectedFileDetectionTest(unittest.TestCase):

  def testScriptIsAffectedBy(self):
    script_path = Path(
        tests_helpers._TOOLS_METRICS_DIR).joinpath('my_script.py')
    relative_script_path = script_path.relative_to(
        tests_helpers._CHROMIUM_SRC_DIR)
    dep_file_path = Path(tests_helpers._TOOLS_METRICS_DIR).joinpath('dep.py')
    unrelated_file_path = Path(
        tests_helpers._TOOLS_METRICS_DIR).joinpath('unrelated.py')
    script = tests_helpers.TestableScript.CreatePythonScript(
        relative_script_path)
    deps = {
        str(script_path.relative_to(tests_helpers._CHROMIUM_SRC_DIR)):
        [dep_file_path.relative_to(tests_helpers._CHROMIUM_SRC_DIR)]
    }

    # Direct modification of the script
    self.assertTrue(
        tests_helpers._is_script_affected_by(script, {script_path}, deps))

    # Indirect modification via dependency
    self.assertTrue(
        tests_helpers._is_script_affected_by(script, {dep_file_path}, deps))

    # Not affected
    self.assertFalse(
        tests_helpers._is_script_affected_by(script, {unrelated_file_path},
                                             deps))

  @patch('chromium_src.tools.metrics.python_support.tests_helpers.'
         '_is_script_affected_by')
  def testGetAffectedTestableScripts(self, mock_is_affected):
    # Set up the mock to return True only for the first script in
    # _TESTABLE_SCRIPTS
    def _is_testable_script_0(script, _, __):
      return script == tests_helpers._TESTABLE_SCRIPTS[0]

    mock_is_affected.side_effect = _is_testable_script_0

    affected = tests_helpers.get_affected_testable_scripts(
        {Path('some_file.py')}, {})

    self.assertEqual(len(affected), 1)
    self.assertEqual(affected[0], tests_helpers._TESTABLE_SCRIPTS[0])

  @patch(
      'chromium_src.tools.metrics.python_support.tests_helpers.find_all_tests')
  @patch('chromium_src.tools.metrics.python_support.tests_helpers.'
         '_is_script_affected_by')
  def testGetAffectedTests(self, mock_is_affected, mock_find_tests):
    mock_find_tests.return_value = [
        os.path.join(tests_helpers._TOOLS_METRICS_DIR, 'test_a.py'),
        os.path.join(tests_helpers._TOOLS_METRICS_DIR, 'test_b.py'),
        os.path.join('/some/outside/dir', 'test_c.py'),
    ]

    def _is_test_a(script, _, __):
      return 'test_a.py' in str(script.file_path)

    mock_is_affected.side_effect = _is_test_a

    affected = tests_helpers.get_affected_tests({Path('mod.py')}, {})

    self.assertEqual(len(affected), 1)
    self.assertIn('test_a.py', str(affected[0].file_path))


if __name__ == '__main__':
  unittest.main()
