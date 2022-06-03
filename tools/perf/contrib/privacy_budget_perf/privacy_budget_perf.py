# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import blink_perf
from telemetry import benchmark


# pylint: disable=protected-access
@benchmark.Info(
    emails=['caraitto@chromium.org', 'asanka@chromium.org'],
    documentation_url='https://bit.ly/blink-perf-benchmarks')
class PrivacyBudgetPerf(blink_perf._BlinkPerfBenchmark):
  SUBDIR = 'privacy_budget'
  TAGS = blink_perf._BlinkPerfBenchmark.TAGS + ['all']

  @classmethod
  def Name(cls):
    return 'blink_perf.privacy_budget'
