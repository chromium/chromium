#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for fetch_coverage_metrics.py."""

import base64
import contextlib
import io
import json
import sys
import textwrap
import unittest
from unittest import mock
import requests
from pyfakefs import fake_filesystem_unittest
import fetch_coverage_metrics as fcm


class FetchCoverageMetricsTest(fake_filesystem_unittest.TestCase):
  """Tests percentage calculation, network helpers, and main CLI workflow."""

  def setUp(self) -> None:
    """Sets up virtual filesystem and network mocks for test cases."""
    self.setUpPyfakefs()
    self.get_patcher = mock.patch('requests.get')
    self.mock_get = self.get_patcher.start()
    self.addCleanup(self.get_patcher.stop)

    self.make_patcher = mock.patch('fetch_coverage_metrics.make_request')
    self.mock_make = self.make_patcher.start()
    self.addCleanup(self.make_patcher.stop)

    self.lines_patcher = mock.patch(
      'fetch_coverage_metrics.fetch_gerrit_file_lines'
    )
    self.mock_lines = self.lines_patcher.start()
    self.addCleanup(self.lines_patcher.stop)

  def test_parse_percentage_dict(self) -> None:
    """Verifies ratio dict percentage calculation."""
    val = {'covered': 4, 'total': 5}
    self.assertEqual(fcm.parse_percentage(val), 80.0)

  def test_parse_percentage_dict_zero_total(self) -> None:
    """Verifies zero total ratio dict defaults to 100%."""
    val = {'covered': 0, 'total': 0}
    self.assertEqual(fcm.parse_percentage(val), 100.0)

  def test_parse_percentage_scalar(self) -> None:
    """Verifies numeric scalar percentage parsing."""
    self.assertEqual(fcm.parse_percentage(65.5), 65.5)
    self.assertEqual(fcm.parse_percentage('42.0'), 42.0)

  def test_parse_percentage_invalid(self) -> None:
    """Verifies invalid values return None."""
    self.assertIsNone(fcm.parse_percentage(None))
    self.assertIsNone(fcm.parse_percentage('not-a-num'))

  def test_make_request_success(self) -> None:
    """Verifies make_request sends Authorization header."""
    self.make_patcher.stop()
    mock_resp = mock.MagicMock()
    mock_resp.content = b'{"data": {}}'
    self.mock_get.return_value = mock_resp

    res = fcm.make_request('https://example.com/api', 'my-token')
    self.assertEqual(res, b'{"data": {}}')
    self.mock_get.assert_called_once_with(
      'https://example.com/api',
      headers={'Authorization': 'Bearer my-token'},
      timeout=30,
    )

  def test_make_request_http_error(self) -> None:
    """Verifies RequestException is caught and logged."""
    self.make_patcher.stop()
    self.mock_get.side_effect = requests.RequestException('404 not found')
    buf = io.StringIO()
    with contextlib.redirect_stderr(buf):
      res = fcm.make_request('https://example.com/api')
    self.assertIsNone(res)
    self.assertIn('Error fetching', buf.getvalue())

  def test_fetch_gerrit_file_lines_success(self) -> None:
    """Verifies base64 decoding of Gerrit file content."""
    self.lines_patcher.stop()
    raw_b64 = base64.b64encode(b'line 1\nline 2')
    self.mock_make.return_value = raw_b64
    lines = fcm.fetch_gerrit_file_lines('host', 1, 1, 'path/foo.cc', 'tok')
    self.assertEqual(lines, {'1': 'line 1', '2': 'line 2'})
    self.mock_make.assert_called_once()

  def test_fetch_gerrit_file_lines_no_data(self) -> None:
    """Verifies missing request data returns empty dict."""
    self.lines_patcher.stop()
    self.mock_make.return_value = None
    lines = fcm.fetch_gerrit_file_lines('host', 1, 1, 'path/foo.cc')
    self.assertEqual(lines, {})

  def test_fetch_gerrit_file_lines_decode_error(self) -> None:
    """Verifies base64 decoding errors return empty dict and log."""
    self.lines_patcher.stop()
    self.mock_make.return_value = b'!!!not_base64!!!'
    buf = io.StringIO()
    with contextlib.redirect_stderr(buf):
      lines = fcm.fetch_gerrit_file_lines('host', 1, 1, 'path/foo.cc')
    self.assertEqual(lines, {})
    self.assertIn('Error decoding file', buf.getvalue())

  def test_main_success_stdout(self) -> None:
    """Verifies full CLI workflow printing JSON to stdout."""
    pct_json = {
      'data': {
        'files': [
          {
            'path': 'partial_cov.cc',
            'absolute_coverage': {'covered': 85, 'total': 100},
          },
          {
            'path': 'high_cov.cc',
            'absolute_coverage': {'covered': 10, 'total': 10},
          },
        ]
      }
    }
    lines_json = {
      'data': {
        'files': [
          {
            'path': 'partial_cov.cc',
            'lines': [
              {'line': 10, 'count': 0},
              {'line': 20, 'count': 5},
            ],
          }
        ]
      }
    }
    self.mock_make.side_effect = [
      json.dumps(pct_json).encode('utf-8'),
      json.dumps(lines_json).encode('utf-8'),
    ]
    self.mock_lines.return_value = {'10': 'uncovered();', '20': 'cov();'}

    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '123',
      '--patchset',
      '1',
    ]
    out_buf = io.StringIO()
    with mock.patch.object(sys, 'argv', test_args):
      with contextlib.redirect_stdout(out_buf):
        fcm.main()

    expected_json = textwrap.dedent("""\
        {
          "partial_cov.cc": {
            "metrics": {
              "absolute_coverage": 85.0
            },
            "low_coverage_type": [
              "absolute_coverage"
            ],
            "uncovered_lines": {
              "10": "uncovered();"
            }
          },
          "high_cov.cc": {
            "metrics": {
              "absolute_coverage": 100.0
            },
            "low_coverage_type": [],
            "uncovered_lines": {}
          }
        }""")
    self.assertEqual(out_buf.getvalue().strip(), expected_json)

  def test_main_success_file_output(self) -> None:
    """Verifies CLI workflow saving report to disk via pathlib."""
    pct_json = {'data': {'files': {'foo.java': {'absolute_coverage': 10.0}}}}
    lines_json = {'data': {'files': {'foo.java': {'lines': []}}}}
    self.mock_make.side_effect = [
      json.dumps(pct_json).encode('utf-8'),
      json.dumps(lines_json).encode('utf-8'),
    ]
    self.mock_lines.return_value = {}

    out_path = '/mock/out/report.json'
    self.fs.create_dir('/mock/out')
    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '1',
      '--patchset',
      '1',
      '--output',
      out_path,
    ]
    with mock.patch.object(sys, 'argv', test_args):
      fcm.main()

    saved = self.fs.get_object(out_path).contents
    self.assertIn('"foo.java"', saved)

  def test_main_files_filtering(self) -> None:
    """Verifies CLI --files argument filters out non-matching files."""
    pct_json = {
      'data': {
        'files': {
          'foo.java': {'absolute_coverage': 10.0},
          'bar.cc': {'absolute_coverage': 20.0},
        }
      }
    }
    lines_json = {'data': {'files': {'foo.java': {'lines': []}}}}
    self.mock_make.side_effect = [
      json.dumps(pct_json).encode('utf-8'),
      json.dumps(lines_json).encode('utf-8'),
    ]
    self.mock_lines.return_value = {}

    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '1',
      '--patchset',
      '1',
      '--files',
      'foo.java',
    ]
    out_buf = io.StringIO()
    with mock.patch.object(sys, 'argv', test_args):
      with contextlib.redirect_stdout(out_buf):
        fcm.main()

    res = json.loads(out_buf.getvalue())
    self.assertIn('foo.java', res)
    self.assertNotIn('bar.cc', res)

  def test_main_percentages_fetch_failure(self) -> None:
    """Verifies exit(1) when percentages fetch returns None."""
    self.mock_make.return_value = None
    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '1',
      '--patchset',
      '1',
    ]
    err_buf = io.StringIO()
    with mock.patch.object(sys, 'argv', test_args):
      with contextlib.redirect_stderr(err_buf):
        with self.assertRaises(SystemExit) as cm:
          fcm.main()
    self.assertEqual(cm.exception.code, 1)
    self.assertIn('Failed to fetch', err_buf.getvalue())

  def test_main_percentages_json_failure(self) -> None:
    """Verifies exit(1) when percentages JSON parsing fails."""
    self.mock_make.return_value = b'not_json'
    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '1',
      '--patchset',
      '1',
    ]
    err_buf = io.StringIO()
    with mock.patch.object(sys, 'argv', test_args):
      with contextlib.redirect_stderr(err_buf):
        with self.assertRaises(SystemExit) as cm:
          fcm.main()
    self.assertEqual(cm.exception.code, 1)
    self.assertIn('Failed to parse', err_buf.getvalue())

  def test_main_lines_query_error(self) -> None:
    """Verifies lines query exception logs to stderr and continues."""
    pct_json = {'data': {'files': {'bar.cc': {'absolute_coverage': 0.0}}}}
    self.mock_make.side_effect = [
      json.dumps(pct_json).encode('utf-8'),
      b'bad_lines_json',
    ]
    test_args = [
      'fcm.py',
      '--host',
      'h',
      '--project',
      'p',
      '--change',
      '1',
      '--patchset',
      '1',
    ]
    err_buf = io.StringIO()
    out_buf = io.StringIO()
    with mock.patch.object(sys, 'argv', test_args):
      with contextlib.redirect_stderr(err_buf):
        with contextlib.redirect_stdout(out_buf):
          fcm.main()
    self.assertIn('Error processing lines', err_buf.getvalue())
    self.assertIn('"uncovered_lines": {}', out_buf.getvalue())


if __name__ == '__main__':
  unittest.main()
