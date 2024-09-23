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
    with self.assertLogs('basic_logger', level=logging.DEBUG) as info_log:
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
        self.assertIn('DEBUG:basic_logger:' + line, info_log.output)


class LegacyOutputAdapterTests(unittest.TestCase):

  def testNoRecipeEngineOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.INFO) as root_log:
      with self.assertLogs('basic_logger', level=logging.DEBUG) as info_log:
        fake_output = """@@@SEED_STEP@fake_step@@@
@@@STEP_CURSOR@fake_step@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake std_out text
@@@STEP_CLOSED@@@"""
        for line in fake_output.split('\n'):
          adapter.ProcessLine(line)
        self.assertEqual(root_log.output,
                         ['INFO:root:\n[cyan]Running: fake_step[/]'])
        self.assertEqual(info_log.output,
                         ['INFO:basic_logger:fake std_out text'])

  def testStepNameProcessor(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.DEBUG) as root_log:
      with self.assertLogs('basic_logger', level=logging.DEBUG) as info_log:
        fake_output = """@@@SEED_STEP@isolate tests@@@
@@@STEP_CURSOR@isolate tests@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake std_out text
@@@STEP_CLOSED@@@"""
        for line in fake_output.split('\n'):
          adapter.ProcessLine(line)
        self.assertEqual(root_log.output,
                         ['DEBUG:root:\n[cyan]Running: isolate tests[/]'])
        self.assertEqual(info_log.output,
                         ['DEBUG:basic_logger:fake std_out text'])

  def testTaskTrigger(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('basic_logger', level=logging.INFO) as info_log:
      fake_output = """@@@SEED_STEP@test_pre_run.[trigger] fake_test (2)@@@
@@@STEP_CURSOR@test_pre_run.[trigger] fake_test (2)@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LINK@task UI: fake_suite/fakeos/1234567890123456/fake_builder/-1:0:32@https://chromium-swarm.appspot.com/task?id=1234567890123456@@@
fake std_out text
@@@STEP_CLOSED@@@"""
      for line in fake_output.split('\n'):
        adapter.ProcessLine(line)
      self.assertEqual(
          info_log.output,
          [('INFO:basic_logger:Triggered fake_test (2): '
            'https://chromium-swarm.appspot.com/task?id=1234567890123456'),
           'INFO:basic_logger:fake std_out text'])
      self.assertIn('fake_test', adapter._step_to_processors)
      self.assertIn('fake_test', adapter._step_to_log_level)

  def testCollectTasks(self):
    # pylint: disable=line-too-long
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.INFO) as root_log:
      with self.assertLogs('single_line_logger') as collect_steps_log:
        fake_output = """@@@SEED_STEP@collect tasks.wait for tasks@@@
@@@STEP_CURSOR@collect tasks.wait for tasks@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111", "22222"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111", "22222"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
0000-00-00 00:00:00,884 - root: [INFO] prpc cmd: prpc call chromium-swarm.appspot.com swarming.v2.Tasks.ListTaskStates, stdin: {"task_id": ["1111"]}
0000-00-00 00:00:00,660 - root: [INFO] sleeping for 15 seconds
@@@STEP_CLOSED@@@"""
        for line in fake_output.split('\n'):
          adapter.ProcessLine(line)
        self.assertEqual(
            root_log.output,
            ['INFO:root:\n[cyan]Running: collect tasks.wait for tasks[/]'])
        # The ninja statuses are sent to another logger to remove new lines
        log_prefix = 'INFO:single_line_logger:\x1b[2K\r'
        self.assertEqual(collect_steps_log.output, [
            log_prefix + 'Still waiting on: 2 shard(s).',
            log_prefix + 'Still waiting on: 2 shard(s)..',
            log_prefix + 'Still waiting on: 1 shard(s)...',
            log_prefix + 'Still waiting on: 1 shard(s)....',
            log_prefix + 'Still waiting on: 1 shard(s).....',
            log_prefix + 'Still waiting on: 1 shard(s).',
            log_prefix + 'Still waiting on: 1 shard(s)..',
            log_prefix + 'Still waiting on: 0 shard(s)...'
        ])

  def testResultOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('basic_logger', level=logging.DEBUG) as info_log:
      fake_output = """@@@SEED_STEP@test_pre_run.[trigger] fake_test@@@
@@@STEP_CURSOR@test_pre_run.[trigger] fake_test@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
@@@STEP_CLOSED@@@
@@@SEED_STEP@fake_test@@@
@@@STEP_CURSOR@fake_test@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
@@@STEP_LINK@shard #0 test results@https://fake-url.com@@@
@@@STEP_LINK@shard #0 (runtime (13m 0s) + overhead (29s): 13m 29s)@https://faker-link.com@@@
@@@STEP_CLOSED@@@"""
      for line in fake_output.split('\n'):
        adapter.ProcessLine(line)
      self.assertEqual(info_log.output, [
          'DEBUG:basic_logger:Test results for fake_test shard #0: '
          'https://fake-url.com'
      ])

  def testInfoStepLevelOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.INFO) as root_log:
      with self.assertLogs('basic_logger', level=logging.INFO) as info_log:
        fake_output = """@@@SEED_STEP@generate_build_files@@@
@@@STEP_CURSOR@generate_build_files@@@
@@@STEP_STARTED@@@
@@@SET_BUILD_PROPERTY@fake_property@"value"@@@
@@@STEP_LOG_LINE@run_recipe@fake_step_log_line@@@
@@@STEP_LOG_END@run_recipe@@@
@@@STEP_LOG_END@memory_profile@@@
fake info level std_out text
@@@STEP_CLOSED@@@"""
        for line in fake_output.split('\n'):
          adapter.ProcessLine(line)
        self.assertEqual(root_log.output, [
            'INFO:root:\n[cyan]Running: generate_build_files[/]',
        ])
        self.assertEqual(info_log.output,
                         ['INFO:basic_logger:fake info level std_out text'])

  def testCompileStepOutput(self):
    adapter = output_adapter.LegacyOutputAdapter()
    with self.assertLogs('', level=logging.DEBUG) as root_log:
      with self.assertLogs('basic_logger', level=logging.DEBUG) as info_log:
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
RBE Stats: down 0 B, up 0 B,
@@@STEP_CLOSED@@@"""
          for line in fake_output.split('\n'):
            adapter.ProcessLine(line)
          self.assertEqual(root_log.output, [
              'INFO:root:\n[cyan]Running: compile[/]',
          ])
          self.assertEqual(info_log.output, [
              'INFO:basic_logger:Proxy started successfully.',
              'INFO:basic_logger:ninja: Entering directory fake/out',
              'INFO:basic_logger:',
              'INFO:basic_logger:RBE Stats: down 0 B, up 0 B,'
          ])
          # The ninja statuses are sent to another logger to remove new lines
          self.assertEqual(compile_steps_log.output, [
              'INFO:single_line_logger:\x1b[2K\r[1/2] ACTION fake_action',
              'INFO:single_line_logger:\x1b[2K\r[2/2] ACTION fake_action'
          ])


if __name__ == '__main__':
  unittest.main()
