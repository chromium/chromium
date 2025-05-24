#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import json
import sys
import os
from unittest.mock import patch, MagicMock

# We need to adjust the path to import the script directly.
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

import decisiongraph_invoker as dgi
import requests


class TestDecisionGraphInvoker(unittest.TestCase):

  def setUp(self):
    self.config_data = {
        "build_id": "12345",
        "change": 123456,
        "patchset": 1,
        "builder": "chromium-commit-queue",
        "api_key": "test_api_key_123"
    }
    self.config_file_path = "test_config.json"
    self.api_url_with_key = f"{dgi.API_URL}?key={self.config_data['api_key']}"

  def tearDown(self):
    if os.path.exists(self.config_file_path):
      os.remove(self.config_file_path)

  def _create_mock_config_file(self, data):
    """Helper to create a mock JSON config file."""
    with open(self.config_file_path, "w", encoding="utf-8") as f:
      json.dump(data, f)

  @patch('builtins.print')
  def test_load_config_from_json_success(self, mock_print):
    self._create_mock_config_file(self.config_data)
    config = dgi.load_config_from_json(self.config_file_path)
    self.assertEqual(config, self.config_data)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_file_not_found(self, mock_print, mock_exit):
    dgi.load_config_from_json("non_existent_file.json")
    mock_print.assert_any_call(
        "Error: Configuration file not found at non_existent_file.json")
    mock_exit.assert_called_with(1)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_missing_args(self, mock_print, mock_exit):
    incomplete_config = {"build_id": "123", "change": 1}
    self._create_mock_config_file(incomplete_config)
    dgi.load_config_from_json(self.config_file_path)
    mock_print.assert_any_call(
        f"Error: Missing required arguments in JSON config file.")
    mock_exit.assert_called_with(1)

  @patch('sys.exit')
  @patch('builtins.print')
  def test_load_config_from_json_invalid_type(self, mock_print, mock_exit):
    invalid_config = self.config_data.copy()
    invalid_config["change"] = "not_an_int"
    self._create_mock_config_file(invalid_config)
    dgi.load_config_from_json(self.config_file_path)
    mock_print.assert_any_call(
        "Error: Invalid data type for 'change' or 'patchset' in JSON."
        " Expected integers.")
    mock_exit.assert_called_with(1)

  @patch('builtins.print')
  @patch('requests.post')
  def test_fetch_api_data_success(self, mock_post, mock_print):
    mock_response = MagicMock()
    mock_response.status_code = 200
    mock_response.json.return_value = {"status": "success"}
    mock_response.text = '{"status": "success"}'
    mock_response.raise_for_status.return_value = None

    mock_post.return_value = mock_response

    response = dgi.fetch_api_data(self.api_url_with_key, {})
    self.assertEqual(response, {"status": "success"})
    mock_print.assert_any_call('{"status": "success"}')
    mock_print.assert_any_call(200)

  @patch('builtins.print')
  @patch('requests.post')
  def test_fetch_api_data_failure_http_error(self, mock_post, mock_print):
    # Create a mock response object for HTTPError.
    mock_response = MagicMock()
    mock_response.status_code = 500
    mock_response.text = "Internal Server Error"
    mock_response.raise_for_status.side_effect = (
        requests.exceptions.HTTPError("500 Server Error"))

    mock_post.return_value = mock_response

    response = dgi.fetch_api_data(self.api_url_with_key, {})
    self.assertIsNone(response)
    mock_print.assert_any_call(f"An error occurred: 500 Server Error")
    mock_print.assert_any_call("Internal Server Error")
    mock_print.assert_any_call(500)

  @patch('builtins.print')
  @patch('requests.post')
  def test_fetch_api_data_failure_request_exception(self, mock_post,
                                                    mock_print):
    # Simulate a network error (e.g., timeout, connection refused).
    mock_post.side_effect = requests.exceptions.ConnectionError(
        "Max retries exceeded")

    response = dgi.fetch_api_data(self.api_url_with_key, {})
    self.assertIsNone(response)
    mock_print.assert_called_with("An error occurred: Max retries exceeded")

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  @patch('builtins.print')
  def test_main_single_batch_success(self, mock_print, mock_fetch_api_data,
                                     mock_load_config_from_json, mock_exit):
    mock_load_config_from_json.return_value = self.config_data
    mock_fetch_api_data.return_value = {"output": [{"result": "success"}]}

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        'unit_tests', '--sts-config-file', self.config_file_path
    ]):
      dgi.main()
      mock_fetch_api_data.assert_called_once()
      mock_exit.assert_called_with(0)

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  @patch('builtins.print')
  def test_main_multiple_batches_success(self, mock_print, mock_fetch_api_data,
                                         mock_load_config_from_json, mock_exit):
    mock_load_config_from_json.return_value = self.config_data
    mock_fetch_api_data.return_value = {"output": [{"result": "success"}]}

    # Simulate command-line arguments for multiple batches.
    test_targets = [f"test_{i}" for i in range(dgi.BATCH_SIZE + 2)]  # 2 batches
    with patch('sys.argv', ['decisiongraph_invoker.py', '--test-targets'] +
               test_targets + ['--sts-config-file', self.config_file_path]):
      dgi.main()
      self.assertEqual(mock_fetch_api_data.call_count, 2)  # Expect 2 calls
      mock_exit.assert_called_with(0)

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  @patch('builtins.print')
  def test_main_api_failure(self, mock_print, mock_fetch_api_data,
                            mock_load_config_from_json, mock_exit):
    mock_load_config_from_json.return_value = self.config_data
    mock_fetch_api_data.return_value = None  # Simulate API failure.

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        '--sts-config-file', self.config_file_path
    ]):
      dgi.main()
      mock_fetch_api_data.assert_called_once()
      mock_print.assert_any_call("Failed to fetch data from the API.")
      mock_exit.assert_called_with(1)

  @patch('sys.exit')
  @patch('decisiongraph_invoker.load_config_from_json')
  @patch('decisiongraph_invoker.fetch_api_data')
  @patch('builtins.print')
  def test_main_canonical_builder_conversion(self, mock_print,
                                             mock_fetch_api_data,
                                             mock_load_config_from_json,
                                             mock_exit):
    config_with_suffix = self.config_data.copy()
    config_with_suffix["builder"] = "linux-test-selection"
    mock_load_config_from_json.return_value = config_with_suffix
    mock_fetch_api_data.return_value = {"output": [{"result": "success"}]}

    with patch('sys.argv', [
        'decisiongraph_invoker.py', '--test-targets', 'browser_tests',
        '--sts-config-file', self.config_file_path
    ]):
      dgi.main()
      _, kwargs = mock_fetch_api_data.call_args
      payload = kwargs['json_payload']
      self.assertEqual(
          payload['input'][0]['input'][0]['checks'][0]['identifier']
          ['luci_test']['builder'], "linux")
      mock_exit.assert_called_with(0)


if __name__ == '__main__':
  unittest.main()
