#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
import sys

_HERE_DIR = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_DIR, '..', '..', '..', '..'))
_NODE_PATH = os.path.join(_SRC_PATH, 'third_party', 'node')
sys.path.append(_NODE_PATH)

import node


def _check_clang_format_works():
  import subprocess

  clang_format_py = os.path.join(
    _SRC_PATH, 'third_party', 'depot_tools', 'clang_format.py'
  )

  try:
    env = os.environ.copy()
    env['CHROMIUM_BUILDTOOLS_PATH'] = os.path.join(_SRC_PATH, 'buildtools')
    subprocess.run(
      ['python3', clang_format_py, '--version'],
      stdout=subprocess.DEVNULL,
      stderr=subprocess.DEVNULL,
      env=env,
      check=True,
    )
    return True
  except (subprocess.SubprocessError, OSError):
    return False


class LitTemplateFormatterTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self._out_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    os.environ['CHROMIUM_BUILDTOOLS_PATH'] = os.path.join(
      _SRC_PATH, 'buildtools'
    )

    if not _check_clang_format_works():
      self.assertEqual(
        sys.platform,
        'darwin',
        'clang-format.py should always be executable on non-macOS',
      )
      raise unittest.SkipTest(
        'depot_tools/clang_format.py is not working on this host'
      )

  def tearDown(self):
    shutil.rmtree(self._out_dir)

  def _read_file(self, path):
    with open(path, 'r', encoding='utf-8') as file:
      return file.read()

  def _run_formatter(self, args):
    formatter_script = os.path.join(
      _HERE_DIR, "lit_template_formatter", "main.js"
    )
    return node.RunNode([formatter_script] + args)

  # When expected_filename is None, compares the formatted output directly
  # against the original source file, verifying that correct formatting remains
  # unaltered.
  def _run_test(self, filename, expected_filename=None):
    src_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", filename
    )
    expected_path = os.path.join(
      _HERE_DIR,
      "tests",
      "lit_template_formatter",
      "expected" if expected_filename else "",
      expected_filename or filename,
    )

    expected_contents = self._read_file(expected_path)

    # Copy to temp dir for in-place processing
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(src_path, dest_path)

    # First run: format the input
    self._run_formatter([dest_path])
    actual_contents = self._read_file(dest_path)
    self.assertMultiLineEqual(expected_contents, actual_contents)

    # Second run (Idempotency check): re-run formatter on output
    self._run_formatter([dest_path])
    idempotent_contents = self._read_file(dest_path)
    self.assertMultiLineEqual(expected_contents, idempotent_contents)

  def testBasicExpressions(self):
    self._run_test(
      "test_basic_expressions.html.ts", "test_basic_expressions.html.ts"
    )

  def testConditionalAndMap(self):
    self._run_test(
      "test_conditional_and_map.html.ts", "test_conditional_and_map.html.ts"
    )

  def testNestedTemplate(self):
    self._run_test(
      "test_nested_template.html.ts", "test_nested_template.html.ts"
    )

  def testConditionalWithMoreWrapping(self):
    self._run_test(
      "test_conditional_with_more_wrapping.html.ts",
      "test_conditional_with_more_wrapping.html.ts",
    )

  def testWhitespaceSensitiveSiblings(self):
    self._run_test("test_whitespace_sensitive_siblings.html.ts")

  def testCommentsNoWhitespace(self):
    self._run_test("test_comments_no_whitespace.html.ts")

  def testMultilineSubtemplateExpression(self):
    self._run_test(
      "test_multiline_subtemplate_expression.html.ts",
      "test_multiline_subtemplate_expression.html.ts",
    )

  def testLongConditionalSubtemplate(self):
    self._run_test(
      "test_long_conditional_subtemplate.html.ts",
      "test_long_conditional_subtemplate.html.ts",
    )

  def testWithIfExpr(self):
    self._run_test("test_with_if_expr.html.ts", "test_with_if_expr.html.ts")

  def testMultilineAttributeExpression(self):
    self._run_test(
      "test_multiline_attribute_expression.html.ts",
      "test_multiline_attribute_expression.html.ts",
    )

  def testRetainNewlines(self):
    self._run_test("retain_newlines.html.ts", "retain_newlines.html.ts")

  def testSingleLineTemplate(self):
    self._run_test(
      "test_single_line_template.html.ts", "test_single_line_template.html.ts"
    )

  def testMultilineTagWithTextChild(self):
    self._run_test(
      "test_multiline_tag_with_text_child.html.ts",
      "test_multiline_tag_with_text_child.html.ts",
    )

  def testDryRunModeFormatted(self):
    filename = "test_basic_expressions.html.ts"
    expected_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", "expected", filename
    )
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(expected_path, dest_path)
    # Should not throw
    self._run_formatter(["--dry-run", dest_path])

  def testDryRunModeUnformatted(self):
    filename = "test_basic_expressions.html.ts"
    src_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", filename
    )
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(src_path, dest_path)
    # Should throw because it is not formatted
    with self.assertRaises(RuntimeError) as context:
      self._run_formatter(["--dry-run", dest_path])
    self.assertIn("is not formatted", str(context.exception))
    self.assertIn("exit=2", str(context.exception))

  def testDiffModeFormatted(self):
    filename = "test_basic_expressions.html.ts"
    expected_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", "expected", filename
    )
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(expected_path, dest_path)
    # Already formatted, so no diff should show.
    stdout = self._run_formatter(["--diff", dest_path])
    self.assertEqual("", stdout)

  def testDiffModeUnformatted(self):
    filename = "test_basic_expressions.html.ts"
    src_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", filename
    )
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(src_path, dest_path)
    # Should NOT throw, and should return the diff in stdout
    stdout = self._run_formatter(["--diff", dest_path])
    # Should contain the diff in stdout
    self.assertIn("--- ", stdout)
    self.assertIn("+++ ", stdout)
    self.assertIn("-    <h1>${this.title}</h1>", stdout)
    self.assertIn("+  <h1>${this.title}</h1>", stdout)

  def testDiffAndDryRunModeUnformatted(self):
    filename = "test_basic_expressions.html.ts"
    src_path = os.path.join(
      _HERE_DIR, "tests", "lit_template_formatter", filename
    )
    dest_path = os.path.join(self._out_dir, filename)
    shutil.copy(src_path, dest_path)
    # Should throw because it has diffs (and exits with 2 due to dry-run/diff)
    with self.assertRaises(RuntimeError) as context:
      self._run_formatter(["--diff", "--dry-run", dest_path])
    self.assertIn("exit=2", str(context.exception))
    # Should contain the diff in stdout due to --diff flag.
    self.assertIn("stdout:\n", str(context.exception))
    self.assertIn("-    <h1>${this.title}</h1>", str(context.exception))
    self.assertIn("+  <h1>${this.title}</h1>", str(context.exception))

  def testJsUnitTests(self):
    test_script = os.path.join(
      _HERE_DIR, "lit_template_formatter", "lit_template_formatter_test.js"
    )
    node.RunNode([test_script])

  def testMultipleFiles(self):
    filenames = [
      "test_basic_expressions.html.ts",
      "test_comments_no_whitespace.html.ts",
      "test_multiline_attribute_expression.html.ts",
      "retain_newlines.html.ts",
    ]
    dest_paths = []
    expected_contents = []
    for filename in filenames:
      src_path = os.path.join(
        _HERE_DIR, "tests", "lit_template_formatter", filename
      )
      expected_path = os.path.join(
        _HERE_DIR, "tests", "lit_template_formatter", "expected", filename
      )
      if not os.path.exists(expected_path):
        expected_path = src_path
      dest_path = os.path.join(self._out_dir, filename)
      shutil.copy(src_path, dest_path)
      dest_paths.append(dest_path)
      expected_contents.append(self._read_file(expected_path))

    # Format multiple files
    self._run_formatter(dest_paths)

    for dest_path, expected in zip(dest_paths, expected_contents):
      actual = self._read_file(dest_path)
      self.assertMultiLineEqual(expected, actual)


if __name__ == "__main__":
  unittest.main()
