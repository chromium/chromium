#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for get_task_digest.py."""

import contextlib
import io
import json
import sys
import unittest
from unittest import mock
import get_task_digest as gtd


class GetTaskDigestTest(unittest.TestCase):
  """Unit tests for get_task_digest.py."""

  def setUp(self) -> None:
    """Sets up subprocess run mocks."""
    self.run_patcher = mock.patch('subprocess.run')
    self.mock_run = self.run_patcher.start()
    self.addCleanup(self.run_patcher.stop)

  def test_input_properties_success(self) -> None:
    """Verifies hash extraction from input swarm_hashes."""
    data = {
        'input': {
            'properties': {
                'swarm_hashes': {
                    'net_unittests': 'abc123/810'
                }
            }
        }
    }
    mock_proc = mock.MagicMock(returncode=0, stdout=json.dumps(data))
    self.mock_run.return_value = mock_proc
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertEqual(res, 'abc123')

  def test_output_properties_success(self) -> None:
    """Verifies fallback to output properties when input misses."""
    data_in = {'input': {'properties': {'swarm_hashes': {}}}}
    data_out = {'output': {'properties': {'net_unittests': 'def456/999'}}}
    proc_in = mock.MagicMock(returncode=0, stdout=json.dumps(data_in))
    proc_out = mock.MagicMock(returncode=0, stdout=json.dumps(data_out))
    self.mock_run.side_effect = [proc_in, proc_out]
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertEqual(res, 'def456')

  def test_output_swarm_hashes(self) -> None:
    """Verifies extraction from output swarm_hashes."""
    data_in = {'input': {'properties': {}}}
    data_out = {
        'output': {
            'properties': {
                'swarm_hashes': {
                    'net_unittests': 'hash111/50'
                }
            }
        }
    }
    proc_in = mock.MagicMock(returncode=0, stdout=json.dumps(data_in))
    proc_out = mock.MagicMock(returncode=0, stdout=json.dumps(data_out))
    self.mock_run.side_effect = [proc_in, proc_out]
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertEqual(res, 'hash111')

  def test_output_swarming_trigger_properties(self) -> None:
    """Verifies extraction from swarming_trigger_properties."""
    data_in = {'input': {'properties': {}}}
    data_out = {
        'output': {
            'properties': {
                'swarming_trigger_properties': {
                    'swarm_hashes': {
                        'net_unittests': 'hash789/100'
                    }
                }
            }
        }
    }
    proc_in = mock.MagicMock(returncode=0, stdout=json.dumps(data_in))
    proc_out = mock.MagicMock(returncode=0, stdout=json.dumps(data_out))
    self.mock_run.side_effect = [proc_in, proc_out]
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertEqual(res, 'hash789')

  def test_orchestrator_child_build(self) -> None:
    """Verifies fallback to steps scan for child build."""
    data_in1 = {'input': {'properties': {}}}
    data_out1 = {'output': {'properties': {}}}
    data_steps = {
        'steps': [{
            'summaryMarkdown': '* [8000000000000000002](url)'
        }]
    }
    data_in2 = {'input': {'properties': {}}}
    data_out2 = {'output': {'properties': {'net_unittests': 'childhash/999'}}}
    proc_in1 = mock.MagicMock(returncode=0, stdout=json.dumps(data_in1))
    proc_out1 = mock.MagicMock(returncode=0, stdout=json.dumps(data_out1))
    proc_steps = mock.MagicMock(returncode=0, stdout=json.dumps(data_steps))
    proc_in2 = mock.MagicMock(returncode=0, stdout=json.dumps(data_in2))
    proc_out2 = mock.MagicMock(returncode=0, stdout=json.dumps(data_out2))
    self.mock_run.side_effect = [
        proc_in1, proc_out1, proc_steps, proc_in2, proc_out2
    ]
    res = gtd.get_digest_from_properties('orch', 'net_unittests')
    self.assertEqual(res, 'childhash')

  def test_bad_json_handling(self) -> None:
    """Verifies JSONDecodeError handling across bb calls."""
    proc_bad = mock.MagicMock(returncode=0, stdout='invalid json')
    self.mock_run.side_effect = [proc_bad, proc_bad, proc_bad]
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertIsNone(res)

  def test_bb_cli_failure(self) -> None:
    """Verifies handling when bb CLI returns non-zero code."""
    proc_fail = mock.MagicMock(returncode=1, stdout='')
    self.mock_run.side_effect = [proc_fail, proc_fail, proc_fail]
    res = gtd.get_digest_from_properties('123', 'net_unittests')
    self.assertIsNone(res)

  @mock.patch('get_task_digest.get_digest_from_properties', return_value=None)
  def test_main_not_found(self, _) -> None:
    """Verifies sys.exit(1) when digest is missing."""
    err_buf = io.StringIO()
    with mock.patch.object(sys, 'argv',
                           ['gtd.py', '--build', '1', '--step', 's']):
      with contextlib.redirect_stderr(err_buf):
        with self.assertRaises(SystemExit) as cm:
          gtd.main()
    self.assertEqual(cm.exception.code, 1)
    self.assertIn('not found', err_buf.getvalue())

  @mock.patch('get_task_digest.get_digest_from_properties',
              return_value='hash789')
  def test_main_success(self, _) -> None:
    """Verifies digest is printed to stdout."""
    out_buf = io.StringIO()
    with mock.patch.object(sys, 'argv',
                           ['gtd.py', '--build', '1', '--step', 's']):
      with contextlib.redirect_stdout(out_buf):
        gtd.main()
    self.assertIn('hash789', out_buf.getvalue())


if __name__ == '__main__':
  unittest.main()
