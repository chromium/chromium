# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import v8_helper
from contrib.cluster_telemetry import loading_base_ct
from telemetry.web_perf import timeline_based_measurement


# pylint: disable=protected-access
class V8LoadingClusterTelemetry(loading_base_ct._LoadingBaseClusterTelemetry):
  @classmethod
  def Name(cls):
    return 'v8.loading.cluster_telemetry'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    v8_helper.AugmentOptionsForV8Metrics(options,
                                         enable_runtime_call_stats=False)
    return options
