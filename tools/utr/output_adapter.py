# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adapter for printing legacy recipe output"""

import logging
import re
import sys


class PassthroughAdapter:
  """Doesn't filter anything, just logs everything from the recipe run."""

  def ProcessLine(self, line):
    logging.log(logging.DEBUG, line)


class LegacyOutputAdapter:
  """Filters out the @@@ logs from the legacy recipe runner."""

  def __init__(self):
    self._ninja_status_re = re.compile(r'\[(\d+)\/(\d+)\]')
    self._current_proccess_fn = self._DefaultProcessLine
    self._step_to_processors = {
        'compile': self._ProcessCompileLine,
        'reclient compile': self._ProcessCompileLine,
    }
    self._step_to_log_level = {
        'compile': logging.INFO,
        'reclient compile': logging.INFO,
        'generate_build_files': logging.INFO,
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

  def _DefaultProcessLine(self, line):
    if not line.startswith('@@@'):
      # Pass through any non-engine text
      logging.log(self._current_log_level, line)

  def _ProcessCompileLine(self, line):
    matches = self._ninja_status_re.match(line)
    if matches:
      self._single_line_logger.log(self._current_log_level, '\33[2K\r' + line)
      if matches.group(1) == matches.group(2):
        logging.log(self._current_log_level, '')
      return
    self._DefaultProcessLine(line)

  def ProcessLine(self, line):
    # If we're in a new step see if it needs to be parsed differently
    if line.startswith('@@@STEP_CURSOR@'):
      step_name = line[len('@@@STEP_CURSOR@'):-len('@@@')]
      self._current_proccess_fn = self._step_to_processors.get(
          step_name, self._DefaultProcessLine)
      self._current_log_level = self._step_to_log_level.get(
          step_name, logging.DEBUG)
    self._current_proccess_fn(line)
    self._last_line = line
