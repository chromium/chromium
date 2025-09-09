#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for tools.android.build_speed.benchmark."""

import contextlib
import pathlib
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

import benchmark
from pylib import constants


class TestBenchmarkScript(unittest.TestCase):
    """Unit tests for benchmark.py."""

    @unittest.mock.patch('benchmark._terminate_build_server_if_needed')
    @unittest.mock.patch('random.randint', return_value=123)
    @unittest.mock.patch(
        'time.time',
        side_effect=[1.0, 3.5, 5.0, 10.0, 11.0, 13.0, 15.0, 20.5, 22.0, 25.0])
    @unittest.mock.patch('subprocess.run')
    @unittest.mock.patch('benchmark._emulator',
                         return_value=contextlib.nullcontext(
                             unittest.mock.Mock(serial='emulator-5554')))
    def test_run_benchmarks(self, mock_emulator, mock_subprocess_run,
                            mock_time, mock_randint,
                            mock_terminate_build_server):
        benchmarks_to_run = ['chrome_nosig']
        gn_args = ['is_debug=true']
        repeat = 1

        change_file_path = (
            benchmark._SRC_ROOT /
            benchmark._BENCHMARK_FROM_NAME['chrome_nosig'].change_file)
        original_content = change_file_path.read_text()

        try:
            with tempfile.TemporaryDirectory() as tmpdir:
                out_dir = pathlib.Path(tmpdir)
                timings = benchmark.run_benchmarks(
                    benchmarks_to_run,
                    gn_args,
                    out_dir,
                    'target',
                    repeat,
                    emulator_avd_name='emulator.avd')
                self.assertEqual(timings['gn_gen'], [2.5])
                self.assertEqual(timings['chrome_nosig_compile'], [5.5])
                self.assertEqual(timings['chrome_nosig_install'], [3.0])
        finally:
            # Ensure the file is restored even if the test fails.
            change_file_path.write_text(original_content)

        self.assertEqual(change_file_path.read_text(), original_content)

    @unittest.mock.patch('builtins.print')
    @unittest.mock.patch('benchmark.run_benchmarks')
    def test_main_logic(self, mock_run_benchmarks, mock_print):
        mock_run_benchmarks.return_value = {
            'gn_gen': [1.23],
            'chrome_nosig_compile': [10.56]
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            constants.SetOutputDirectory(tmpdir)

            # Test default args and quiet mode.
            with unittest.mock.patch(
                    'sys.argv', ['benchmark.py', 'chrome_nosig', '--quiet']):
                benchmark.main()
            args, _kwargs = mock_run_benchmarks.call_args
            expected_gn_args = [
                'target_os="android"',
                'use_remoteexec=true',
                'use_siso=true',
                'android_static_analysis="build_server"',
                'incremental_install=true',
                'target_cpu="x86"',
            ]
            self.assertEqual(args[1], expected_gn_args)
            # The real output is tested in the next block, this just clears
            # the mock calls.
            mock_print.reset_mock()

            # Test --no-server and output printing.
            with unittest.mock.patch(
                    'sys.argv',
                ['benchmark.py', 'chrome_nosig', '--no-server']):
                benchmark.main()
            args, _kwargs = mock_run_benchmarks.call_args
            expected_gn_args = [
                'target_os="android"',
                'use_remoteexec=true',
                'use_siso=true',
                'android_static_analysis="on"',
                'incremental_install=true',
                'target_cpu="x86"',
            ]
            self.assertEqual(args[1], expected_gn_args)
            # Get the printed output from the mock.
            printed_output = '\n'.join(c.args[0]
                                       for c in mock_print.call_args_list)
            expected_output = (
                'Summary\n'
                'emulator: None\n'
                'gn args: target_os="android" use_remoteexec=true '
                'use_siso=true android_static_analysis="on" '
                'incremental_install=true target_cpu="x86"\n'
                'target: chrome_public_apk\n'
                'gn_gen: 1.2s\n'
                'chrome_nosig_compile: 10.6s')
            self.assertEqual(printed_output, expected_output)


if __name__ == '__main__':
    unittest.main()
