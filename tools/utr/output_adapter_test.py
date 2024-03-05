#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for output_adapter.py"""

import io
import logging
import unittest
from unittest import mock

import output_adapter


class PassthroughAdapterTests(unittest.TestCase):

  def testBasic(self):
    adapter = output_adapter.PassthroughAdapter()
    with self.assertLogs('', level=logging.DEBUG) as info_log:
      fake_output = """@@@SEED_STEP@fake_step@@@
@@@STEP_CURSOR@fake_step@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake std_out text"""
      lines = fake_output.split('\n')
      for line in lines:
        adapter.ProcessLine(line)
      for line in lines:
        self.assertIn('DEBUG:root:' + line, info_log.output)


class LegacyOutputAdapterTests(unittest.TestCase):

  def testNoRecipeEngineOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.DEBUG) as info_log:
      fake_output = """@@@SEED_STEP@fake_step@@@
@@@STEP_CURSOR@fake_step@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake std_out text"""
      for line in fake_output.split('\n'):
        adapter.ProcessLine(line)
      self.assertEqual(info_log.output, ['DEBUG:root:fake std_out text'])

  def testInfoStepLevelOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.INFO) as info_log:
      fake_output = """@@@SEED_STEP@generate_build_files@@@
@@@STEP_CURSOR@generate_build_files@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake info level std_out text"""
      for line in fake_output.split('\n'):
        adapter.ProcessLine(line)
      self.assertEqual(info_log.output,
                       ['INFO:root:fake info level std_out text'])

  def testCompileStepOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.DEBUG) as info_log:
      with self.assertLogs('single_line_logger') as compile_steps_log:
        fake_output = """@@@SEED_STEP@compile@@@
@@@STEP_CURSOR@compile@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
Proxy started successfully.
ninja: Entering directory fake/out
[1/2] ACTION fake_action
[2/2] ACTION fake_action
RBE Stats: down 0 B, up 0 B,"""
        for line in fake_output.split('\n'):
          adapter.ProcessLine(line)
        self.assertEqual(info_log.output, [
            'INFO:root:Proxy started successfully.',
            'INFO:root:ninja: Entering directory fake/out', 'INFO:root:',
            'INFO:root:RBE Stats: down 0 B, up 0 B,'
        ])
        # The ninja statuses are sent to another logger to remove new lines
        self.assertEqual(compile_steps_log.output, [
            'INFO:single_line_logger:\x1b[2K\r[1/2] ACTION fake_action',
            'INFO:single_line_logger:\x1b[2K\r[2/2] ACTION fake_action'
        ])


if __name__ == '__main__':
  unittest.main()
