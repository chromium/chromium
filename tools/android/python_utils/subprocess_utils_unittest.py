#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for subprocess_utils."""

import os
import pathlib
import subprocess
import unittest

import subprocess_utils

_COMMAND_PROCESS_ERROR_LOG_REGEX = (r'Command ".*" failed with code \d+\.'
                                    r'(\nSTDERR: .*)?(\nSTDOUT: .*)?')


class TestRunCommand(unittest.TestCase):
    """Tests for the run_command function."""

    def test_run_command_no_args(self):
        expected_cwd: str = os.getcwd()

        pwd_output = subprocess_utils.run_command(['pwd'])

        self.assertEqual(expected_cwd, pwd_output)

    def test_run_command_custom_cwd(self):
        expected_cwd = '/usr/local/bin'

        pwd_output = subprocess_utils.run_command(['pwd'], cwd=expected_cwd)

        self.assertEqual(expected_cwd, pwd_output)

    def test_run_command_custom_cwd_path(self):
        expected_cwd = '/usr/local/bin'

        pwd_output = subprocess_utils.run_command(
            ['pwd'], cwd=pathlib.Path(expected_cwd).resolve(strict=True))

        self.assertEqual(expected_cwd, pwd_output)

    def test_run_command_multiple_args(self):
        expected_output = 'some sample output'

        command_output = subprocess_utils.run_command(
            ['echo', 'some', 'sample', 'output'])

        self.assertEqual(expected_output, command_output)

    def test_run_command_strips_output_whitespace(self):
        expected_output = 'Hello, world!'

        command_output = subprocess_utils.run_command(
            ['echo', '\t Hello, world! \n'])

        self.assertEqual(expected_output, command_output)

    def test_run_command_exitcode_only_returns_zero_on_success(self):
        expected_output = 0

        command_output = subprocess_utils.run_command(
            ['echo', '\t Hello, world! \n'], exitcode_only=True)

        self.assertEqual(expected_output, command_output)

    def test_run_command_exitcode_only_returns_exitcode_on_failure(self):
        expected_output = 1

        command_output = subprocess_utils.run_command(['git', 'foo'],
                                                      exitcode_only=True)

        self.assertEqual(expected_output, command_output)

    def test_run_command_process_error_no_output(self):
        with self.assertRaises(
                subprocess.CalledProcessError) as error_cm, self.assertLogs(
                    level='ERROR') as logging_cm:
            subprocess_utils.run_command(['false'])

        self.assertEqual(1, error_cm.exception.returncode)
        self.assertEqual(1, len(logging_cm.output))
        self.assertRegex(logging_cm.output[0],
                         _COMMAND_PROCESS_ERROR_LOG_REGEX)

    def test_run_command_process_error_with_stderr_output(self):
        expected_stderr_regex = r"git: 'foo' is not a git command.+"

        with self.assertRaises(
                subprocess.CalledProcessError) as error_cm, self.assertLogs(
                    level='ERROR') as logging_cm:
            subprocess_utils.run_command(['git', 'foo'])

        self.assertEqual(1, error_cm.exception.returncode)
        self.assertRegex(error_cm.exception.stderr, expected_stderr_regex)
        self.assertEqual('', error_cm.exception.stdout)
        self.assertEqual(1, len(logging_cm.output))
        self.assertRegex(logging_cm.output[0],
                         _COMMAND_PROCESS_ERROR_LOG_REGEX)

    def test_run_command_process_error_with_stdout_output(self):
        expected_stdout_regex = r'usage: git.+'

        with self.assertRaises(
                subprocess.CalledProcessError) as error_cm, self.assertLogs(
                    level='ERROR') as logging_cm:
            subprocess_utils.run_command(['git'])

        self.assertEqual(1, error_cm.exception.returncode)
        self.assertEqual('', error_cm.exception.stderr)
        self.assertRegex(error_cm.exception.stdout, expected_stdout_regex)
        self.assertEqual(1, len(logging_cm.output))
        self.assertRegex(logging_cm.output[0],
                         _COMMAND_PROCESS_ERROR_LOG_REGEX)

    def test_run_command_not_found(self):
        with self.assertRaises(FileNotFoundError) as error_cm:
            subprocess_utils.run_command(['foo_cmd'])

        self.assertEqual(2, error_cm.exception.errno)
        self.assertEqual('foo_cmd', error_cm.exception.filename)


if __name__ == '__main__':
    unittest.main()
