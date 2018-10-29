# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.testing import tab_test_case
from telemetry.timeline import tracing_config
from tracing.trace_data import trace_data


def GetSyncEvents(trace_part):
  return [x for x in trace_part if x['ph'] == 'c']

class ClockDomainTest(tab_test_case.TabTestCase):

  # Don't run this test on Android and remote Chrome OS; it won't work
  # because there are two different devices, so the clock domains will
  # be different.
  @decorators.Disabled('android', 'cros-chrome')
  @decorators.Isolated
  def testTelemetryUsesChromeClockDomain(self):

    tracing_controller = self._browser.platform.tracing_controller
    options = tracing_config.TracingConfig()
    options.enable_chrome_trace = True
    tracing_controller.StartTracing(options)

    full_trace = tracing_controller.StopTracing()[0]

    chrome_sync = GetSyncEvents(
        full_trace.GetTraceFor(trace_data.CHROME_TRACE_PART)['traceEvents'])
    telemetry_sync = GetSyncEvents(
        full_trace.GetTraceFor(trace_data.TELEMETRY_PART)['traceEvents'])

    assert len(chrome_sync) == 1, 'Expected 1 saw %s' % len(chrome_sync)
    assert len(telemetry_sync) == 1 , 'Expected 1 saw %s' % len(telemetry_sync)

    # If Telemetry and Chrome are in the same clock domain, the Chrome sync
    # timestamp should be between Telemetry's sync start and end timestamps.
    ts_telemetry_start = telemetry_sync[0]['args']['issue_ts']
    ts_chrome = chrome_sync[0]['ts']
    ts_telemetry_end = telemetry_sync[0]['ts']
    assert ts_chrome > ts_telemetry_start
    assert ts_telemetry_end > ts_chrome
