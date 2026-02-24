# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from . import constants

from opentelemetry import trace

sys.path.append(str(constants.DEPOT_TOOLS_DIR / 'infra_lib'))
import telemetry

tracer = telemetry.get_tracer(__name__)

# ------------------------------------------------------------------------------
# Telemetry tracks attributes declared in main, build, and run phases.
# Note: Telemetry only applies to Googlers only
# For details, see: go/autotest-telemetry
# ------------------------------------------------------------------------------


def RecordMainAttributes(targets: list[str], gtest_filter: str,
                         used_cache: bool, out_dir: str):
  """Records main attributes to the current span.

  Attributes recorded:
      * main.is_gemini_cli: Indicates if the process is running via the Gemini CLI.
      * main.targets: The list of targets selected for build/run.
      * main.filter: The filter string generated from user input.
      * gn.target_cache_used: Whether the target search utilized the cache.
      * build.out_dir: The directory path for compiled artifacts.
  """
  span = trace.get_current_span()
  if not span.is_recording():
    return

  is_gemini_cli = os.getenv("GEMINI_CLI", "") == "1"

  span.set_attribute('main.is_gemini_cli', is_gemini_cli)
  span.set_attribute('main.targets', str(targets))
  span.set_attribute('main.filter', gtest_filter)
  span.set_attribute('gn.target_used_cache', used_cache)
  span.set_attribute('build.out_dir', out_dir)


def RecordBuildAttributes(is_retry: str, is_successful: bool):
  """Records build execution state attributes to the current span.

  Attributes recorded:
      * build.is_retry: Indicates if this build is a retry attempt.
      * build.is_successful: Indicates return code of subprocess run
  """
  span = trace.get_current_span()
  if not span.is_recording():
    return

  span.set_attribute('build.is_retry', is_retry)
  span.set_attribute('build.is_successful', is_successful)


def RecordRunAttributes(cmd: list[str], is_successful: bool):
  """Records attributes related to the command execution.

  Attributes recorded:
      * run.bin: The specific binary name extracted from the full command path.
      * run.is_successful: Indicates return code of subprocess run
  """
  span = trace.get_current_span()
  if not span.is_recording():
    return

  run_bin = os.path.basename(cmd[0])
  span.set_attribute('run.bin', run_bin)
  span.set_attribute('run.is_successful', is_successful)
  if not is_successful:
    span.set_status(trace.StatusCode.ERROR)
