# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core.results_processor import command_line

from telemetry.testing import options_for_unittests


def GetRunOptions(*args, **kwargs):
  """Augment telemetry options for tests with results_processor defaults."""
  options = options_for_unittests.GetRunOptions(*args, **kwargs)
  parser = command_line.ArgumentParser()
  processor_options = parser.parse_args([])
  for arg in vars(processor_options):
    if not hasattr(options, arg):
      setattr(options, arg, getattr(processor_options, arg))
  return options
