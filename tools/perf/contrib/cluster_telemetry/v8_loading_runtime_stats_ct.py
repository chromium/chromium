# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import v8_browsing
from contrib.cluster_telemetry import loading_base_ct
from telemetry.web_perf import timeline_based_measurement

# pylint: disable=protected-access
class V8LoadingRuntimeStatsClusterTelemetry(
    loading_base_ct._LoadingBaseClusterTelemetry):

  @classmethod
  def Name(cls):
    return 'v8.loading_runtime_stats.cluster_telemetry'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    v8_browsing.AugmentOptionsForV8BrowsingMetrics(options,
        enable_runtime_call_stats=True)
    return options
