#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for tools.android.build_speed.timeall."""

import subprocess
import unittest
import unittest.mock

import timeall


class TimeallTest(unittest.TestCase):

    def setUp(self):
        self.original_quiet_state = timeall._QUIET
        timeall._QUIET = True

    def tearDown(self):
        timeall._QUIET = self.original_quiet_state

    @unittest.mock.patch('subprocess.run')
    def test_run_benchmark_basic(self, mock_run):
        mock_run.return_value = unittest.mock.Mock(stdout='output',
                                                   stderr='',
                                                   check=True)
        options = timeall._Options(benchmark='chrome_nosig',
                                   r=1,
                                   e='emulator.avd',
                                   i=True,
                                   n=False,
                                   s=True)
        timeall._run_benchmark(options)

        expected_cmd = [
            'tools/android/build_speed/benchmark.py',
            '-vv',
            'chrome_nosig',
            '-C',
            'out/Debug',
            '--target',
            'chrome_apk',
            '--emulator',
            'emulator.avd',
        ]
        mock_run.assert_called_once_with(expected_cmd,
                                         capture_output=True,
                                         text=True,
                                         check=True)

    @unittest.mock.patch('subprocess.run')
    def test_run_benchmark_test_target(self, mock_run):
        mock_run.return_value = unittest.mock.Mock(stdout='output',
                                                   stderr='',
                                                   check=True)
        options = timeall._Options(benchmark='cta_test_sig',
                                   r=1,
                                   e='emulator.avd',
                                   i=True,
                                   n=False,
                                   s=True)
        timeall._run_benchmark(options)

        expected_cmd = [
            'tools/android/build_speed/benchmark.py',
            '-vv',
            'cta_test_sig',
            '-C',
            'out/Debug',
            '--target',
            'chrome_test_apk',
            '--emulator',
            'emulator.avd',
        ]
        mock_run.assert_called_once_with(expected_cmd,
                                         capture_output=True,
                                         text=True,
                                         check=True)

    @unittest.mock.patch('subprocess.run')
    def test_run_benchmark_all_flags_off_and_no_emulator(self, mock_run):
        mock_run.return_value = unittest.mock.Mock(stdout='output',
                                                   stderr='',
                                                   check=True)
        options = timeall._Options(benchmark='base_sig',
                                   r=1,
                                   e='',
                                   i=False,
                                   n=True,
                                   s=False)
        timeall._run_benchmark(options)

        expected_cmd = [
            'tools/android/build_speed/benchmark.py',
            '-vv',
            'base_sig',
            '-C',
            'out/Debug',
            '--target',
            'chrome_apk',
            '--build-64bit',
            '--no-incremental-install',
            '--no-component-build',
            '--no-server',
        ]
        mock_run.assert_called_once_with(expected_cmd,
                                         capture_output=True,
                                         text=True,
                                         check=True)

    @unittest.mock.patch('subprocess.run')
    def test_run_command_with_repeat_no_failure(self, mock_run):
        mock_run.return_value = unittest.mock.Mock(stdout='output',
                                                   stderr='',
                                                   check=True)
        cmd = ['some', 'command']
        output = timeall._run_command_with_repeat(cmd,
                                                  repeat=3,
                                                  outdir_name='out/Debug')
        self.assertEqual(output, 'output')
        mock_run.assert_called_once_with(cmd,
                                         capture_output=True,
                                         text=True,
                                         check=True)

    @unittest.mock.patch('subprocess.run')
    def test_run_command_with_repeat_one_failure(self, mock_run):
        cmd = ['some', 'command']
        gn_clean_cmd = ['gn', 'clean', 'out/Debug']

        mock_run.side_effect = [
            subprocess.CalledProcessError(1, cmd, '', ''),
            unittest.mock.Mock(stdout='', stderr='', check=True),
            unittest.mock.Mock(stdout='output', stderr='', check=True),
        ]

        output = timeall._run_command_with_repeat(cmd,
                                                  repeat=3,
                                                  outdir_name='out/Debug')
        self.assertEqual(output, 'output')
        self.assertEqual(mock_run.call_count, 3)
        mock_run.assert_has_calls([
            unittest.mock.call(cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(gn_clean_cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(cmd,
                                capture_output=True,
                                text=True,
                                check=True),
        ])

    @unittest.mock.patch('subprocess.run')
    def test_run_command_with_repeat_all_failures(self, mock_run):
        cmd = ['some', 'command']
        gn_clean_cmd = ['gn', 'clean', 'out/Debug']

        def side_effect(command, **kwargs):
            if command == cmd:
                raise subprocess.CalledProcessError(1, cmd, '', '')
            return unittest.mock.Mock(stdout='', stderr='', check=True)

        mock_run.side_effect = side_effect

        output = timeall._run_command_with_repeat(cmd,
                                                  repeat=3,
                                                  outdir_name='out/Debug')
        self.assertIsNone(output)
        self.assertEqual(mock_run.call_count, 5)
        mock_run.assert_has_calls([
            unittest.mock.call(cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(gn_clean_cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(gn_clean_cmd,
                                capture_output=True,
                                text=True,
                                check=True),
            unittest.mock.call(cmd,
                                capture_output=True,
                                text=True,
                                check=True),
        ])

    @unittest.mock.patch('timeall.run')
    def test_main_debug(self, mock_run):
        with unittest.mock.patch('sys.argv', ['timeall.py', '--debug']):
            timeall.main()
        mock_run.assert_called_once_with(True)

    @unittest.mock.patch('timeall.run')
    def test_main_no_debug(self, mock_run):
        with unittest.mock.patch('sys.argv', ['timeall.py']):
            timeall.main()
        mock_run.assert_called_once_with(False)

    @unittest.mock.patch('timeall.run')
    def test_main_quiet(self, mock_run):
        timeall._QUIET = False  # Override setUp for this test.
        with unittest.mock.patch('sys.argv', ['timeall.py', '--quiet']):
            timeall.main()
        self.assertTrue(timeall._QUIET)

    @unittest.mock.patch('timeall._run_benchmarks')
    def test_run_debug(self, mock_run_benchmarks):
        timeall.run(debug=True)
        mock_run_benchmarks.assert_called_once()
        _, kwargs = mock_run_benchmarks.call_args
        benchmark_options = kwargs['benchmark_options']
        self.assertEqual(len(benchmark_options), 1)
        options = benchmark_options[0]
        self.assertEqual(options.benchmark, 'module_internal_nosig')
        self.assertEqual(options.r, 1)
        self.assertEqual(options.e, 'android_34_google_apis_x64_local.textpb')
        self.assertTrue(options.i)
        self.assertTrue(options.n)
        self.assertTrue(options.s)

    @unittest.mock.patch('random.choice',
                         return_value='android_31_google_apis_x64_local.textpb')
    @unittest.mock.patch('random.shuffle', side_effect=lambda x: x)
    @unittest.mock.patch('timeall._run_benchmarks')
    def test_run_no_debug(self, mock_run_benchmarks, mock_shuffle,
                          mock_choice):
        timeall.run(debug=False)
        mock_run_benchmarks.assert_called_once()
        _, kwargs = mock_run_benchmarks.call_args
        benchmark_options = kwargs['benchmark_options']
        self.assertEqual(len(benchmark_options), 32)

        # Spot check the first and last generated options
        first_options = benchmark_options[0]
        self.assertEqual(first_options.benchmark, 'module_internal_nosig')
        self.assertEqual(first_options.r, 3)
        self.assertEqual(first_options.e,
                         'android_34_google_apis_x64_local.textpb')
        self.assertTrue(first_options.i)
        self.assertTrue(first_options.n)
        self.assertTrue(first_options.s)

        last_options = benchmark_options[-1]
        self.assertEqual(last_options.benchmark, 'cta_test_sig')
        self.assertEqual(last_options.r, 3)
        self.assertEqual(last_options.e,
                         'android_31_google_apis_x64_local.textpb')
        self.assertTrue(last_options.i)
        self.assertFalse(last_options.n)
        self.assertFalse(last_options.s)


if __name__ == '__main__':
    unittest.main()
