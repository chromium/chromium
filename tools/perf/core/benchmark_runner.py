# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point module to run benchmarks with Telemetry and process results.

This wires up together both the Telemetry command line and runner, responsible
of running stories and collecting artifacts (traces, logs, etc.), together with
the results_processor, responsible of processing such artifacts and e.g. run
metrics to produce various kinds of outputs (histograms, csv, results.html).
"""

import os
import platform

from core import results_processor
from telemetry import command_line


def main(config, args=None):
  results_arg_parser = results_processor.ArgumentParser()
  options = command_line.ParseArgs(
      environment=config, args=args, results_arg_parser=results_arg_parser)
  results_processor.ProcessOptions(options)

  # Mac perf testers have a different behaviour when this environment var is
  # set i.e. Chrome is started using 'open' command. See crbug/1454294
  if platform.system() == 'Darwin':
    os.environ['START_BROWSER_WITH_DEFAULT_PRIORITY'] = '1'

  run_return_code = command_line.RunCommand(options)
  process_return_code = results_processor.ProcessResults(options)
  if process_return_code != 0:
    return process_return_code
  return run_return_code
