# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adapter for printing legacy recipe output

TODO(https://crbug.com/326904531): This file is intended to be a temporary
workaround and should be replaced once this bug is resolved"""

import json
import logging
import re
import sys


class PassthroughAdapter:
  """Doesn't filter anything, just logs everything from the recipe run."""

  def ProcessLine(self, line):
    logging.log(logging.DEBUG, line)


class LegacyOutputAdapter:
  """Interprets the legacy recipe run mode output to logging

  This will filter, route and in some cases reformat the output to trace levels
  of logging. This will cause specific output (e.g. unfiltered step names) to
  always print to std out or when -v is passed, the stdout will additionally
  be passed to the logging stdout. Note -vv will cause PassthroughAdapter to
  interpret results"""

  SEED_STEP_TEXT = '@@@SEED_STEP@'
  STEP_CLOSED_TEXT = '@@@STEP_CLOSED@@@'
  ANNOTATOR_PREFIX_SUFIX = '@@@'
  TRIGGER_STEP_PREFIX = 'test_pre_run.[trigger] '
  TRIGGER_LINK_TEXT = '@@@STEP_LINK@task UI:'
  RDB_FINALIZED_LINK = 'rdb-stream: finalized invocation - '

  def __init__(self):
    self._trigger_link_re = re.compile(r'.+@(https://.+)@@@$')
    self._ninja_status_re = re.compile(r'\[(\d+)\/(\d+)\]')
    self._collect_wait_re = re.compile(
        r'.+prpc call (.+) swarming.v2.Tasks.ListTaskStates, stdin: '
        r'(\{"task_id": .+\})$'
    )
    self._result_links_re = re.compile(
        r'@@@STEP_LINK@shard (#\d+) test results@(https://[^@]+)@@@')
    self._current_proccess_fn = self._StepNameProcessLine
    # The first match is used. By default _StepNameProcessLine will be used
    # which prints the step name and it's stdout
    self._step_to_processors = {
        'compile': self._ProcessCompileLine,
        'reclient compile': self._ProcessCompileLine,
        'test_pre_run.[trigger] ': self._ProcessTriggerLine,
        'collect tasks.wait for tasks': self._ProcessCollectLine,
    }
    # The first match is used. By default INFO will be used which prints in
    # non-verbose mode (i.e. no -v flag)
    self._step_to_log_level = {
        'setup_build': logging.DEBUG,
        'get compile targets for scripts': logging.DEBUG,
        'lookup GN args': logging.DEBUG,
        'install infra/tools/luci/isolate': logging.DEBUG,
        'find command lines': logging.DEBUG,
        'test_pre_run.install infra/tools/luci/swarming': logging.DEBUG,
        'isolate tests': logging.DEBUG,
        'read GN args': logging.DEBUG,
        'test_pre_run.[trigger] ': logging.INFO,
        'test_pre_run.': logging.DEBUG,
        'collect tasks.wait for tasks': logging.INFO,
        'collect tasks': logging.DEBUG,
        '$debug - all results': logging.DEBUG,
        'Test statistics': logging.DEBUG,
        'read gclient': logging.DEBUG,
        'write output_properties_file': logging.DEBUG,
    }
    self._last_line = ''
    self._current_log_level = logging.DEBUG
    # Setup logger for printing to the same line
    logger = logging.getLogger('single_line_logger')
    handler = logging.StreamHandler(sys.stdout)
    handler.terminator = ''
    logger.addHandler(handler)
    logger.propagate = False
    self._single_line_logger = logger
    self._current_step_name = ''
    self._dot_count = 0

  def _StdoutProcessLine(self, line):
    if not line.startswith(self.ANNOTATOR_PREFIX_SUFIX):
      # Pass through any non-engine text
      logging.log(self._current_log_level, line)

  def _StepNameProcessLine(self, line):
    if line.startswith(self.SEED_STEP_TEXT):
      # Always print the step name to info
      logging.log(self._current_log_level,
                  '\nRunning: ' + self._current_step_name)
      return
    if not line.startswith(self.ANNOTATOR_PREFIX_SUFIX):
      # Pass through any non-engine text
      logging.log(self._current_log_level, line)

  def _ProcessTriggerLine(self, line):
    if line.startswith(self.SEED_STEP_TEXT + self.TRIGGER_STEP_PREFIX):
      # The step names for tests don't have any identifying keywords so the
      # result step parsers need to be installed at trigger time
      test_name = line[len(self.SEED_STEP_TEXT +
                           self.TRIGGER_STEP_PREFIX):line.index(' (') if ' (' in
                       line else -len(self.ANNOTATOR_PREFIX_SUFIX)]
      self._step_to_processors[test_name] = self._ProcessResult
      self._step_to_log_level[test_name] = logging.DEBUG
    elif line.startswith(self.TRIGGER_LINK_TEXT):
      matches = self._trigger_link_re.match(line)
      if matches:
        task_name = self._current_step_name[len(self.TRIGGER_STEP_PREFIX):]
        logging.log(self._current_log_level,
                    f'Triggered {task_name}: ' + matches[1])
    else:
      self._StdoutProcessLine(line)

  def _ProcessCompileLine(self, line):
    if line.startswith(self.SEED_STEP_TEXT):
      logging.info('\nRunning: ' + self._current_step_name)
      return
    matches = self._ninja_status_re.match(line)
    if matches:
      self._single_line_logger.log(self._current_log_level, '\33[2K\r' + line)
      return
    elif self._last_line.startswith('['):
      logging.log(self._current_log_level, '')
    self._StdoutProcessLine(line)

  def _ProcessCollectLine(self, line):
    if line.startswith(self.SEED_STEP_TEXT):
      logging.info('\nRunning: ' + self._current_step_name)
    matches = self._collect_wait_re.match(line)
    if matches:
      task_ids = json.loads(matches[2])['task_id']
      self._dot_count = (self._dot_count % 5) + 1
      self._single_line_logger.log(
          self._current_log_level,
          f'\33[2K\rStill waiting on: {len(task_ids)} shard(s)' +
          '.' * self._dot_count)
      return
    elif line == self.STEP_CLOSED_TEXT:
      self._single_line_logger.log(self._current_log_level,
                                   '\33[2K\rStill waiting on: 0 shard(s)...')
      logging.log(self._current_log_level, '')

  def _ProcessResult(self, line):
    matches = self._result_links_re.match(line)
    if matches:
      logging.log(
          self._current_log_level,
          f'Test results for {self._current_step_name} shard {matches[1]}: '
          f'{matches[2]}')

  def ProcessLine(self, line):
    # If we're in a new step see if it needs to be parsed differently
    if line.startswith(self.SEED_STEP_TEXT):
      self._current_step_name = line[len(self.SEED_STEP_TEXT
                                         ):-len(self.ANNOTATOR_PREFIX_SUFIX)]
      self._current_proccess_fn = self._get_processor(self._current_step_name)
      self._current_log_level = self._get_log_level(self._current_step_name)
    elif line.startswith(self.RDB_FINALIZED_LINK):
      # The finalized invocation comes from the rdb wrap, not the recipe itself
      # so it can't be handed off to a specific step processor
      link = line[len(self.RDB_FINALIZED_LINK):]
      logging.info(f'Finalized test results: {link}')
    self._current_proccess_fn(line)
    self._last_line = line

  def _get_processor(self, step_name):
    if step_name in self._step_to_processors:
      return self._step_to_processors[step_name]
    else:
      for match_name in self._step_to_processors:
        if step_name.startswith(match_name):
          return self._step_to_processors[match_name]
    return self._StepNameProcessLine

  def _get_log_level(self, step_name):
    if step_name in self._step_to_log_level:
      return self._step_to_log_level[step_name]
    else:
      for match_name in self._step_to_log_level:
        if step_name.startswith(match_name):
          return self._step_to_log_level[match_name]
    return logging.INFO
