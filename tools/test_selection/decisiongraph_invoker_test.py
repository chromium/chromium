#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import json
import sys
import os
import tempfile
import shutil
from unittest.mock import patch, MagicMock

# We need to adjust the path to import the script directly.
# This assumes the test script is in the same directory as the main script.
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

import decisiongraph_invoker as dgi
import requests


class TestDecisionGraphInvoker(unittest.TestCase):

  def setUp(self):
    self.maxDiff = None
    self.test_dir = tempfile.mkdtemp()
    self.config_data = {
        'build_id': '12345',
        'change': 123456,
        'patchset': 1,
        'builder': 'chromium-commit-queue',
        'api_key': 'test_api_key_123'
    }
    self.config_file_path = os.path.join(self.test_dir, 'test_config.json')
    self.api_url_with_key = f"{dgi.API_URL}?key={self.config_data['api_key']}"

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def _create_mock_config_file(self, data):
    """Helper to create a mock JSON config file."""
    with open(self.config_file_path, 'w', encoding='utf-8') as f:
      json.dump(data, f)

  @patch('builtins.print')
  def test_load_config_from_json_success(self, mock_print):
    self._create_mock_config_file(self.config_data)
    config = dgi.load_config_from_json(self.config_file_path)
    self.assertEqual(config, self.config_data)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_file_not_found(self, mock_print, mock_exit):
    dgi.load_config_from_json('non_existent_file.json')
    mock_print.assert_any_call(
        'Error: Configuration file not found at non_existent_file.json')
    mock_exit.assert_called_with(1)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_missing_args(self, mock_print, mock_exit):
    incomplete_config = {'build_id': '123', 'change': 1}
    self._create_mock_config_file(incomplete_config)
    dgi.load_config_from_json(self.config_file_path)
    mock_print.assert_any_call(
        'Error: Missing required arguments in JSON config file.')
    mock_exit.assert_called_with(1)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_invalid_type(self, mock_print, mock_exit):
    invalid_config = self.config_data.copy()
    invalid_config['change'] = 'not_an_int'
    self._create_mock_config_file(invalid_config)
    dgi.load_config_from_json(self.config_file_path)
    mock_print.assert_any_call(
        "Error: Invalid data type for 'change' or 'patchset' in JSON. "
        "Expected integers.")
    mock_exit.assert_called_with(1)

  @patch('builtins.print')
  @patch('requests.post')
  def test_fetch_api_data_success(self, mock_post, mock_print):
    mock_response = MagicMock()
    mock_response.status_code = 200
    mock_response.json.return_value = {'status': 'success'}
    mock_response.text = '{"status": "success"}'
    mock_response.raise_for_status.return_value = None

    mock_post.return_value = mock_response

    response = dgi.fetch_api_data(self.api_url_with_key, {})
    self.assertEqual(response, {'status': 'success'})
    mock_print.assert_any_call('{"status": "success"}')
    mock_print.assert_any_call(200)

  @patch('builtins.print')
  @patch('requests.post')
  def test_fetch_api_data_failure_http_error(self, mock_post, mock_print):
    mock_response = MagicMock()
    mock_response.status_code = 500
    mock_response.text = 'Internal Server Error'
    mock_response.raise_for_status.side_effect = (
        requests.exceptions.HTTPError('500 Server Error'))

    mock_post.return_value = mock_response

    response = dgi.fetch_api_data(self.api_url_with_key, {})
    self.assertIsNone(response)
    mock_print.assert_any_call('An error occurred: 500 Server Error')
    mock_print.assert_any_call('Internal Server Error')
    mock_print.assert_any_call(500)

  def test_overwrite_filter_file_success(self):
    test_suite = 'browser_tests'
    tests_to_skip = ['Test1.testA', 'Test2.testB']
    result = dgi.overwrite_filter_file(self.test_dir, test_suite, tests_to_skip)
    self.assertTrue(result)
    filter_file_path = os.path.join(self.test_dir, f'{test_suite}.filter')
    self.assertTrue(os.path.exists(filter_file_path))
    with open(filter_file_path, 'r', encoding='utf-8') as f:
      content = f.read()
      self.assertIn('-Test1.testA', content)
      self.assertIn('-Test2.testB', content)

  def test_overwrite_filter_file_appends_wildcard(self):
    """Tests that a wildcard '*' is appended to each test name."""
    test_suite = 'browser_tests'
    tests_to_skip = ['TestClassOne', 'TestClassTwo']
    result = dgi.overwrite_filter_file(self.test_dir, test_suite, tests_to_skip)

    self.assertTrue(result)
    filter_file_path = os.path.join(self.test_dir, f'{test_suite}.filter')

    with open(filter_file_path, 'r', encoding='utf-8') as f:
      content = f.read()

    expected_content = ('# A list of tests to be skipped, generated by test'
                        ' selection.\n'
                        '-TestClassOne.*\n'
                        '-TestClassTwo.*\n')
    self.assertEqual(content, expected_content)

  @patch('builtins.print')
  def test_overwrite_filter_file_dir_not_found(self, mock_print):
    result = dgi.overwrite_filter_file('non_existent_dir', 'suite', [])
    self.assertFalse(result)
    mock_print.assert_called_with(
        'Error: Filter file directory not found at non_existent_dir')

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  def test_main_trigger_phase_success(self, mock_fetch, mock_load_config,
                                      mock_exit):
    mock_load_config.return_value = self.config_data
    mock_fetch.return_value = {'output': [{'result': 'success'}]}

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        '--sts-config-file', self.config_file_path, '--test-selection-phase',
        'TRIGGER'
    ]):
      dgi.main()
      mock_fetch.assert_called_once()
      _, kwargs = mock_fetch.call_args
      payload = kwargs['json_payload']
      self.assertTrue(
          payload['graph']['stages'][0]['execution_options']['prepare'])
      mock_exit.assert_called_with(0)

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  def test_main_fetch_phase_success(self, mock_fetch, mock_load_config,
                                    mock_exit):
    mock_load_config.return_value = self.config_data
    mock_fetch.return_value = {
        'outputs': [{
            'checks': [{
                'identifier': {
                    'luciTest': {
                        'testSuite': 'browser_tests'
                    }
                },
                'children': [{
                    'identifier': {
                        'luciTest': {
                            'testId': 'TestClass.testExample1'
                        }
                    }
                }, {
                    'identifier': {
                        'luciTest': {
                            'testId': 'TestClass.testExample2'
                        }
                    }
                }]
            }]
        }]
    }

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        '--sts-config-file', self.config_file_path, '--test-selection-phase',
        'FETCH', '--filter-file-dir', self.test_dir
    ]):
      dgi.main()

      mock_fetch.assert_called_once()
      _, kwargs = mock_fetch.call_args
      payload = kwargs['json_payload']
      self.assertFalse(
          payload['graph']['stages'][0]['execution_options']['prepare'])

      filter_file_path = os.path.join(self.test_dir, 'browser_tests.filter')
      self.assertTrue(os.path.exists(filter_file_path))
      with open(filter_file_path, 'r', encoding='utf-8') as f:
        content = f.read()
        self.assertIn('-TestClass.testExample1', content)
        self.assertIn('-TestClass.testExample2', content)

      mock_exit.assert_called_with(0)

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  def test_main_fetch_phase_key_error(self, mock_fetch, mock_load_config,
                                      mock_exit):
    mock_load_config.return_value = self.config_data
    # Malformed response missing 'testId'
    mock_fetch.return_value = {
        'outputs': [{
            'checks': [{
                'identifier': {
                    'luciTest': {
                        'testSuite': 'browser_tests'
                    }
                },
                'children': [{
                    'identifier': {
                        'luciTest': {}
                    }
                }]
            }]
        }]
    }

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        '--sts-config-file', self.config_file_path, '--test-selection-phase',
        'FETCH', '--filter-file-dir', self.test_dir
    ]):
      with self.assertRaises(KeyError):
        dgi.main()

  @patch('argparse.ArgumentParser.error')
  def test_main_fetch_phase_missing_dir_arg(self, mock_parser_error):
    # Make the mock raise an exception to halt execution, which is what
    # parser.error() does internally by calling sys.exit().
    mock_parser_error.side_effect = SystemExit

    with self.assertRaises(SystemExit):
      with patch('sys.argv', [
          'decisiongraph_invoker.py',
          '--test-targets',
          'browser_tests',
          '--sts-config-file',
          self.config_file_path,
          '--test-selection-phase',
          'FETCH',
      ]):
        dgi.main()

    # Verify that parser.error was called with the correct message.
    mock_parser_error.assert_called_with(
        '--filter-file-dir is required when phase is FETCH.')


if __name__ == '__main__':
  unittest.main()
