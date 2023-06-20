#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for code coverage tools."""

import os
import re
import shutil
import subprocess
import sys
import unittest
import coverage_utils


def _RecursiveDirectoryListing(dirpath):
  """Returns a list of relative paths to all files in a given directory."""
  result = []
  for root, _, files in os.walk(dirpath):
    for f in files:
      result.append(os.path.relpath(os.path.join(root, f), dirpath))
  return result


def _ReadFile(filepath):
  """Returns contents of a given file."""
  with open(filepath) as f:
    return f.read()


class CoverageTest(unittest.TestCase):

  def setUp(self):
    self.maxDiff = 1000
    self.COVERAGE_TOOLS_DIR = os.path.abspath(os.path.dirname(__file__))
    self.COVERAGE_SCRIPT = os.path.join(self.COVERAGE_TOOLS_DIR, 'coverage.py')
    self.COVERAGE_UTILS = os.path.join(self.COVERAGE_TOOLS_DIR,
                                       'coverage_utils.py')

    self.CHROMIUM_SRC_DIR = os.path.dirname(
        os.path.dirname(self.COVERAGE_TOOLS_DIR))
    self.BUILD_DIR = os.path.join(self.CHROMIUM_SRC_DIR, 'out',
                                  'code_coverage_tools_test')

    self.REPORT_DIR_1 = os.path.join(self.BUILD_DIR, 'report1')
    self.REPORT_DIR_1_NO_COMPONENTS = os.path.join(self.BUILD_DIR,
                                                   'report1_no_components')
    self.REPORT_DIR_2 = os.path.join(self.BUILD_DIR, 'report2')
    self.REPORT_DIR_3 = os.path.join(self.BUILD_DIR, 'report3')
    self.REPORT_DIR_4 = os.path.join(self.BUILD_DIR, 'report4')

    self.LLVM_COV = os.path.join(self.CHROMIUM_SRC_DIR, 'third_party',
                                 'llvm-build', 'Release+Asserts', 'bin',
                                 'llvm-cov')

    self.PYTHON = 'python3'
    self.PLATFORM = coverage_utils.GetHostPlatform()
    if self.PLATFORM == 'win32':
      self.LLVM_COV += '.exe'
      self.PYTHON += '.exe'

    # Even though 'is_component_build=false' is recommended, we intentionally
    # use 'is_component_build=true' to test handling of shared libraries.
    self.GN_ARGS = ('use_clang_coverage=true '
                    'dcheck_always_on=true '
                    'ffmpeg_branding=\"ChromeOS\" '
                    'is_component_build=true '
                    'is_debug=false '
                    'proprietary_codecs=true '
                    'use_libfuzzer=true')

    shutil.rmtree(self.BUILD_DIR, ignore_errors=True)

    gn_gen_cmd = ['gn', 'gen', self.BUILD_DIR, '--args=%s' % self.GN_ARGS]
    self.run_cmd(gn_gen_cmd)

    build_cmd = [
        'autoninja', '-C', self.BUILD_DIR, 'crypto_unittests',
        'libpng_read_fuzzer'
    ]
    self.run_cmd(build_cmd)

  def tearDown(self):
    shutil.rmtree(self.BUILD_DIR, ignore_errors=True)

  def run_cmd(self, cmd):
    return subprocess.check_output(cmd, cwd=self.CHROMIUM_SRC_DIR)

  def verify_component_view(self, filepath):
    """Asserts that a given component view looks correct."""
    # There must be several Blink and Internals components.
    with open(filepath) as f:
      data = f.read()

    counts = data.count('Blink') + data.count('Internals')
    self.assertGreater(counts, 5)

  def verify_directory_view(self, filepath):
    """Asserts that a given directory view looks correct."""
    # Directory view page does a redirect to another page, extract its URL.
    with open(filepath) as f:
      data = f.read()

    url = re.search(r'.*refresh.*url=([a-zA-Z0-9_\-\/.]+).*', data).group(1)
    directory_view_path = os.path.join(os.path.dirname(filepath), url)

    # There must be at least 'crypto' and 'third_party' directories.
    with open(directory_view_path) as f:
      data = f.read()

    self.assertTrue('crypto' in data and 'third_party' in data)

  def verify_file_view(self, filepath):
    """Asserts that a given file view looks correct."""
    # There must be hundreds of '.*crypto.*' files and 10+ of '.*libpng.*'.
    with open(filepath) as f:
      data = f.read()

    self.assertGreater(data.count('crypto'), 100)
    self.assertGreater(data.count('libpng'), 10)

  def verify_lcov_file(self, filepath):
    """Asserts that a given lcov file looks correct."""
    with open(filepath) as f:
      data = f.read()

    self.assertGreater(data.count('SF:'), 100)
    self.assertGreater(data.count('crypto'), 100)
    self.assertGreater(data.count('libpng'), 10)

  def test_different_workflows_and_cross_check_the_results(self):
    """Test a few different workflows and assert that the results are the same

    and look legit.
    """
    # Testcase 1. End-to-end report generation using coverage.py script. This is
    # the workflow of a regular user.
    cmd = [
        self.COVERAGE_SCRIPT,
        'crypto_unittests',
        'libpng_read_fuzzer',
        '-v',
        '-b',
        self.BUILD_DIR,
        '-o',
        self.REPORT_DIR_1,
        '-c'
        '%s/crypto_unittests' % self.BUILD_DIR,
        '-c',
        '%s/libpng_read_fuzzer -runs=0 third_party/libpng/' % self.BUILD_DIR,
    ]
    self.run_cmd(cmd)

    output_dir = os.path.join(self.REPORT_DIR_1, self.PLATFORM)
    self.verify_component_view(
        os.path.join(output_dir, 'component_view_index.html'))
    self.verify_directory_view(
        os.path.join(output_dir, 'directory_view_index.html'))
    self.verify_file_view(os.path.join(output_dir, 'file_view_index.html'))

    # Also try generating a report without components view. Useful for cross
    # checking with the report produced in the testcase #3.
    cmd = [
        self.COVERAGE_SCRIPT,
        'crypto_unittests',
        'libpng_read_fuzzer',
        '-v',
        '-b',
        self.BUILD_DIR,
        '-o',
        self.REPORT_DIR_1_NO_COMPONENTS,
        '-c'
        '%s/crypto_unittests' % self.BUILD_DIR,
        '-c',
        '%s/libpng_read_fuzzer -runs=0 third_party/libpng/' % self.BUILD_DIR,
        '--no-component-view',
    ]
    self.run_cmd(cmd)

    output_dir = os.path.join(self.REPORT_DIR_1_NO_COMPONENTS, self.PLATFORM)
    self.verify_directory_view(
        os.path.join(output_dir, 'directory_view_index.html'))
    self.verify_file_view(os.path.join(output_dir, 'file_view_index.html'))
    self.assertFalse(
        os.path.exists(os.path.join(output_dir, 'component_view_index.html')))

    # Testcase #2. Run the script for post processing in Chromium tree. This is
    # the workflow of the code coverage bots.
    instr_profile_path = os.path.join(self.REPORT_DIR_1, self.PLATFORM,
                                      'coverage.profdata')

    cmd = [
        self.COVERAGE_SCRIPT,
        'crypto_unittests',
        'libpng_read_fuzzer',
        '-v',
        '-b',
        self.BUILD_DIR,
        '-p',
        instr_profile_path,
        '-o',
        self.REPORT_DIR_2,
    ]
    self.run_cmd(cmd)

    # Verify that the output dirs are the same except of the expected diff.
    report_1_listing = set(_RecursiveDirectoryListing(self.REPORT_DIR_1))
    report_2_listing = set(_RecursiveDirectoryListing(self.REPORT_DIR_2))
    logs_subdir = os.path.join(self.PLATFORM, 'logs')
    self.assertEqual(
        set([
            os.path.join(self.PLATFORM, 'coverage.profdata'),
            os.path.join(logs_subdir, 'crypto_unittests_output.log'),
            os.path.join(logs_subdir, 'libpng_read_fuzzer_output.log'),
        ]), report_1_listing - report_2_listing)

    output_dir = os.path.join(self.REPORT_DIR_2, self.PLATFORM)
    self.verify_component_view(
        os.path.join(output_dir, 'component_view_index.html'))
    self.verify_directory_view(
        os.path.join(output_dir, 'directory_view_index.html'))
    self.verify_file_view(os.path.join(output_dir, 'file_view_index.html'))

    # Verify that the file view pages are binary equal.
    report_1_file_view_data = _ReadFile(
        os.path.join(self.REPORT_DIR_1, self.PLATFORM, 'file_view_index.html'))
    report_2_file_view_data = _ReadFile(
        os.path.join(self.REPORT_DIR_2, self.PLATFORM, 'file_view_index.html'))
    self.assertEqual(report_1_file_view_data, report_2_file_view_data)

    # Testcase #3, run coverage_utils.py on manually produced report and summary
    # file. This is the workflow of OSS-Fuzz code coverage job.
    objects = [
        '-object=%s' % os.path.join(self.BUILD_DIR, 'crypto_unittests'),
        '-object=%s' % os.path.join(self.BUILD_DIR, 'libpng_read_fuzzer'),
    ]

    cmd = [
        self.PYTHON,
        self.COVERAGE_UTILS,
        '-v',
        'shared_libs',
        '-build-dir=%s' % self.BUILD_DIR,
    ] + objects

    shared_libraries = self.run_cmd(cmd)
    objects.extend(shared_libraries.split())

    instr_profile_path = os.path.join(self.REPORT_DIR_1_NO_COMPONENTS,
                                      self.PLATFORM, 'coverage.profdata')
    cmd = [
        self.LLVM_COV,
        'show',
        '-format=html',
        '-output-dir=%s' % self.REPORT_DIR_3,
        '-instr-profile=%s' % instr_profile_path,
    ] + objects
    if self.PLATFORM in ['linux', 'mac']:
      cmd.extend(['-Xdemangler', 'c++filt', '-Xdemangler', '-n'])
    self.run_cmd(cmd)

    cmd = [
        self.LLVM_COV,
        'export',
        '-summary-only',
        '-instr-profile=%s' % instr_profile_path,
    ] + objects
    summary_output = self.run_cmd(cmd)

    summary_path = os.path.join(self.REPORT_DIR_3, 'summary.json')
    with open(summary_path, 'wb') as f:
      f.write(summary_output)

    cmd = [
        self.PYTHON,
        self.COVERAGE_UTILS,
        '-v',
        'post_process',
        '-src-root-dir=%s' % self.CHROMIUM_SRC_DIR,
        '-summary-file=%s' % summary_path,
        '-output-dir=%s' % self.REPORT_DIR_3,
    ]
    self.run_cmd(cmd)

    output_dir = os.path.join(self.REPORT_DIR_3, self.PLATFORM)
    self.verify_directory_view(
        os.path.join(output_dir, 'directory_view_index.html'))
    self.verify_file_view(os.path.join(output_dir, 'file_view_index.html'))
    self.assertFalse(
        os.path.exists(os.path.join(output_dir, 'component_view_index.html')))

    # Verify that the file view pages are binary equal.
    report_1_file_view_data_no_component = _ReadFile(
        os.path.join(self.REPORT_DIR_1_NO_COMPONENTS, self.PLATFORM,
                     'file_view_index.html'))
    report_3_file_view_data = _ReadFile(
        os.path.join(self.REPORT_DIR_3, self.PLATFORM, 'file_view_index.html'))
    self.assertEqual(report_1_file_view_data_no_component,
                     report_3_file_view_data)

    # Testcase 4. Export coverage data in lcov format using coverage.py script.
    cmd = [
        self.COVERAGE_SCRIPT,
        'crypto_unittests',
        'libpng_read_fuzzer',
        '--format',
        'lcov',
        '-v',
        '-b',
        self.BUILD_DIR,
        '-o',
        self.REPORT_DIR_4,
        '-c'
        '%s/crypto_unittests' % self.BUILD_DIR,
        '-c',
        '%s/libpng_read_fuzzer -runs=0 third_party/libpng/' % self.BUILD_DIR,
    ]
    self.run_cmd(cmd)

    output_dir = os.path.join(self.REPORT_DIR_4, self.PLATFORM)
    self.verify_lcov_file(os.path.join(output_dir, 'coverage.lcov'))


if __name__ == '__main__':
  unittest.main()
