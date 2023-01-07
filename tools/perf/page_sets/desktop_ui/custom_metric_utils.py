# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
This file provides some helper functions to report trace events as metrics
to show in custom metric
third_party/catapult/tracing/tracing/metrics/custom_metric.html
See the following doc for usage
docs/speed/benchmark/harnesses/desktop_ui.md
'''

import json

# Common units
# Definition: third_party/catapult/tracing/tracing/base/unit.html
sizeInBytes_smallerIsBetter = 'sizeInBytes_smallerIsBetter'
sizeInBytes_biggerIsBetter = 'sizeInBytes_biggerIsBetter'
timeDurationInMS_smallerIsBetter = 'ms_smallerIsBetter'
timeDurationInMS_biggerIsBetter = 'ms_biggerIsBetter'
unitlessNumber_smallerIsBetter = 'unitless_smallerIsBetter'
unitlessNumber_biggerIsBetter = 'unitless_biggerIsBetter'


# Set metric names that can be picked up by custom metric.
def SetMetricNames(action_runner, metric_names):
  events = [{'name': name} for name in metric_names]
  SetMetrics(action_runner, events)


# Set metrics that can be picked up by custom metric.
# metrics are in the format of name,
# unit(optional, default to 'ms_smallerIsBetter')
# and description(optional, default to empty).
# {'name':<METRIC_NAME>, 'unit':<UNIT_NAME>, 'description':<DESCRIPTION>}
def SetMetrics(action_runner, metrics):
  action_runner.ExecuteJavaScript(REPORT_TRACE_EVENT_SCRIPT %
                                  json.dumps(metrics))


REPORT_TRACE_EVENT_SCRIPT = '''
performance.mark(`custom_metric:manifest:%s`);
'''
