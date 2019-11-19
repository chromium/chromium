# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point module to run benchmarks with Telemetry and process results.

This wires up together both the Telemetry command line and runner, responsible
of running stories and collecting artifacts (traces, logs, etc.), together with
the results_processor, responsible of processing such artifacts and e.g. run
metrics to produce various kinds of outputs (histograms, csv, results.html).
"""

from core import results_processor
from telemetry import command_line


def main(config, args=None):
  results_arg_parser = results_processor.ArgumentParser()
  options = command_line.ParseArgs(
      environment=config, args=args, results_arg_parser=results_arg_parser)
  results_processor.ProcessOptions(options)
  run_return_code = command_line.RunCommand(options)
  process_return_code = results_processor.ProcessResults(options)
  if process_return_code != 0:
    return process_return_code
  else:
    return run_return_code
