#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for reproduce.py"""
import json
import os
import unittest

import reproduce

# From vpython
import mock


class ReproduceTest(unittest.TestCase):

  @mock.patch('reproduce.fetch_step_log')
  def test_parse_failed_steps_ci_build(self, fetch_log_mock):
    b = reproduce.Build('ci/linux-rel/123')
    b.patch_info = (False,)
    fetch_log_mock.side_effect = [
        json.dumps({
            'canonical_step_name': 'gl_unittests'
        })
    ]
    self.assertEqual(
        b._parse_failed_steps([{
            'name': 'bot update',
            'status': 'FAILURE',
        }, {
            'name': 'base_unittests',
            'status': 'SUCCESS',
            'logs': [{
                'name': 'step_metadata',
            }],
        }, {
            'name': 'gl_unittests',
            'status': 'FAILURE',
            'logs': [{
                'name': 'step_metadata',
            }],
        }]),
        ['gl_unittests'])

  @mock.patch('reproduce.fetch_step_log')
  def test_parse_failed_steps_try_build(self, fetch_log_mock):
    b = reproduce.Build('ci/linux-rel/123')
    b.patch_info = (True,)
    fetch_log_mock.side_effect = [
        json.dumps({
            'canonical_step_name': 'gl_unittests'
        })
    ]
    self.assertEqual(
        b._parse_failed_steps([{
            'name': 'bot update',
            'status': 'FAILURE',
        }, {
            'name': 'base_unittests (with patch)',
            'status': 'FAILURE',
            'logs': [{
                'name': 'step_metadata',
            }],
        }, {
            'name': 'gl_unittests (with patch)',
            'status': 'FAILURE',
            'logs': [{
                'name': 'step_metadata',
            }],
        }, {
            'name': 'base_unittests (retry shards with patch)',
            'status': 'SUCCESS',
            'logs': [{
                'name': 'step_metadata',
            }],
        }, {
            'name': 'gl_unittests (retry shards with patch)',
            'status': 'FAILURE',
            'logs': [{
                'name': 'step_metadata',
            }],
        }]),
        ['gl_unittests'])

  @mock.patch('__builtin__.print')
  @mock.patch('reproduce.guess_host_dimensions')
  @mock.patch('__builtin__.raw_input')
  @mock.patch('reproduce.run_command')
  def test_run_suite_gtest(self, run_mock, input_mock, guess_mock, print_mock):
    # Just want to mock this so if something weird happens with the test it
    # doesn't block for input.
    del input_mock, print_mock

    guess_mock.side_effect = [{
        'os': 'Windows',
    }]
    b = reproduce.Build('luci.chromium.ci/linux-rel/1')
    b._mastername = 'chromium.linux'
    b._test_results = {
        'gl_unittests': ['test.1', 'test.2']
    }
    with mock.patch.object(b, 'guess_swarming_dimensions') as dims_mock:
      dims_mock.side_effect = [{
          'os': 'Windows',
      }]
      with mock.patch.object(b, 'lookup_suite') as lookup_mock:
        entry = reproduce.TestSuiteEntry(
            'gl_unittests',
            [],
            False,
            {}
        )
        lookup_mock.side_effect = [entry]
        b.run_suite('gl_unittests', '//out/Default')

      run_mock.assert_called_with([
          os.path.join(reproduce.CHROMIUM_ROOT, 'tools', 'mb', 'mb.py'),
          'run', '-m', 'chromium.linux', '-b', 'linux-rel', '//out/Default',
          'gl_unittests', '--', '--gtest_filter=test.1:test.2'
      ])

  @mock.patch('__builtin__.print')
  @mock.patch('reproduce.guess_host_dimensions')
  @mock.patch('__builtin__.raw_input')
  @mock.patch('reproduce.run_command')
  def test_run_suite_isolated_script(self, run_mock, input_mock, guess_mock,
                                      print_mock):
    # Just want to mock this so if something weird happens with the test it
    # doesn't block for input.
    del input_mock, print_mock

    guess_mock.side_effect = [{
        'os': 'Windows',
    }]
    b = reproduce.Build('luci.chromium.ci/linux-rel/1')
    b._mastername = 'chromium.mac'
    b._test_results = {
        'blink_web_tests': ['test.1', 'test.2']
    }
    fake_tempfile_name = '/tmp/foobartmp'
    with mock.patch.object(b, 'guess_swarming_dimensions') as dims_mock:
      dims_mock.side_effect = [{
          'os': 'Windows',
      }]
      with mock.patch('reproduce.os.unlink') as unlink_mock:
        with mock.patch.object(b, 'lookup_suite') as lookup_mock:
          entry = reproduce.TestSuiteEntry(
              'blink_web_tests',
              [],
              True,
              {}
          )
          lookup_mock.side_effect = [entry]
          with mock.patch('reproduce.tempfile.mkstemp') as temp_mock:
            # The script tries to close the file handler to the temp file when
            # it creates it. Mock that out.
            with mock.patch('reproduce.os.close'):
                temp_mock.side_effect = [(5, fake_tempfile_name)]
                b.run_suite('blink_web_tests', '//out/Default')

        unlink_mock.assert_called_with(fake_tempfile_name)
        run_mock.assert_called_with([
            os.path.join(reproduce.CHROMIUM_ROOT, 'tools', 'mb', 'mb.py'),
            'run', '-m', 'chromium.mac', '-b', 'linux-rel', '//out/Default',
            'blink_web_tests', '--',
            '--isolated-script-test-filter=test.1::test.2',
            '--isolated-script-test-output', fake_tempfile_name,
        ])

  @mock.patch('__builtin__.print')
  @mock.patch('reproduce.guess_host_dimensions')
  @mock.patch('__builtin__.raw_input')
  @mock.patch('reproduce.run_command')
  def test_run_suite_dimension_prompt(self, run_mock, input_mock, guess_mock,
                                      print_mock):
    guess_mock.side_effect = [{
        'os': 'Windows',
    }]

    b = reproduce.Build('luci.chromium.ci/linux-rel/1')
    input_mock.side_effect = ['n']
    with mock.patch.object(b, 'guess_swarming_dimensions') as dims_mock:
      dims_mock.side_effect = [{
          'os': 'Linux',
      }]
      with mock.patch.object(b, 'lookup_suite') as lookup_mock:
        entry = reproduce.TestSuiteEntry(
            'blink_web_tests',
            [],
            True,
            {}
        )
        lookup_mock.side_effect = [entry]
        result = b.run_suite('blink_web_tests', '//out/Default')
        self.assertEqual(result, 1)
        input_mock.assert_called_with('>> ')
        self.assertEqual(print_mock.call_count, 2)
    run_mock.assert_not_called()

  @mock.patch('__builtin__.print')
  @mock.patch('reproduce.guess_host_dimensions')
  @mock.patch('__builtin__.raw_input')
  @mock.patch('reproduce.run_command')
  def test_run_suite_already_prompted(self, run_mock, input_mock, guess_mock,
                                      print_mock):
    """Makes sure that the script remembers dimension warning bypassing."""
    # Just want to mock these so if something weird happens with the test it
    # doesn't block for input.
    del input_mock, print_mock

    guess_mock.side_effect = [{
        'os': 'Windows',
    }]

    b = reproduce.Build('luci.chromium.ci/linux-rel/1')
    b._mastername = 'chromium.linux'
    b._test_results = {
        'gl_unittests': ['test.1', 'test.2']
    }
    b.acked_dimensions.add('os')
    with mock.patch.object(b, 'guess_swarming_dimensions') as dims_mock:
      dims_mock.side_effect = [{
          'os': 'Linux',
      }]
      with mock.patch.object(b, 'lookup_suite') as lookup_mock:
        entry = reproduce.TestSuiteEntry(
            'gl_unittests',
            [],
            False,
            {}
        )
        lookup_mock.side_effect = [entry]
        b.run_suite('gl_unittests', '//out/Default')

    run_mock.assert_called_with([
        os.path.join(reproduce.CHROMIUM_ROOT, 'tools', 'mb', 'mb.py'),
        'run', '-m', 'chromium.linux', '-b', 'linux-rel', '//out/Default',
        'gl_unittests', '--', '--gtest_filter=test.1:test.2'
    ])

if __name__ == '__main__':
  unittest.main()
