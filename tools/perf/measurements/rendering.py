# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import legacy_page_test
from telemetry.timeline import model as model_module
from telemetry.value import trace
from telemetry.web_perf import smooth_gesture_util
from telemetry.web_perf import timeline_interaction_record as tir_module
from telemetry.timeline import tracing_config

from metrics import timeline
from measurements import rendering_util


def _CollectRecordsFromRendererThreads(model, renderer_thread):
  records = []
  for event in renderer_thread.async_slices:
    if tir_module.IsTimelineInteractionRecord(event.name):
      interaction = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
      # Adjust the interaction record to match the synthetic gesture
      # controller if needed.
      interaction = (
          smooth_gesture_util.GetAdjustedInteractionIfContainGesture(
              model, interaction))
      records.append(interaction)
  return records


class Rendering(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(Rendering, self).__init__()
    self._results = None

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')

  def WillNavigateToPage(self, page, tab):
    config = tracing_config.TracingConfig()
    config.enable_chrome_trace = True
    config.enable_platform_display_trace = True

    # Basic categories for smoothness.
    config.chrome_trace_config.SetLowOverheadFilter()

    # Extra categories from commandline flag.
    if self.options and self.options.extra_chrome_categories:
      config.chrome_trace_config.category_filter.AddFilterString(
          self.options.extra_chrome_categories)

    tab.browser.platform.tracing_controller.StartTracing(config)

  def ValidateAndMeasurePage(self, _, tab, results):
    self._results = results
    tab.browser.platform.tracing_controller.telemetry_info = (
        results.telemetry_info)
    trace_result = tab.browser.platform.tracing_controller.StopTracing()

    # TODO(charliea): This is part of a three-sided Chromium/Telemetry patch
    # where we're changing the return type of StopTracing from a TraceValue to a
    # (TraceValue, nonfatal_exception_list) tuple. Once the tuple return value
    # lands in Chromium, the non-tuple logic should be deleted.
    if isinstance(trace_result, tuple):
      trace_result = trace_result[0]

    trace_value = trace.TraceValue(
        results.current_page, trace_result,
        file_path=results.telemetry_info.trace_local_path,
        remote_path=results.telemetry_info.trace_remote_path,
        upload_bucket=results.telemetry_info.upload_bucket,
        cloud_url=results.telemetry_info.trace_remote_url)
    results.AddValue(trace_value)

    model = model_module.TimelineModel(trace_result)
    renderer_thread = model.GetFirstRendererThread(tab.id)
    records = _CollectRecordsFromRendererThreads(model, renderer_thread)

    thread_times_metric = timeline.ThreadTimesTimelineMetric()
    thread_times_metric.AddResults(model, renderer_thread, records, results)

    rendering_util.AddTBMv2RenderingMetrics(
        trace_value, results, import_experimental_metrics=True)

  def DidRunPage(self, platform):
    if platform.tracing_controller.is_tracing_running:
      trace_result = platform.tracing_controller.StopTracing()
      if self._results:

        # TODO(charliea): This is part of a three-sided Chromium/Telemetry patch
        # where we're changing the return type of StopTracing from a TraceValue
        # to a (TraceValue, nonfatal_exception_list) tuple. Once the tuple
        # return value lands in Chromium, the non-tuple logic should be deleted.
        if isinstance(trace_result, tuple):
          trace_result = trace_result[0]

        trace_value = trace.TraceValue(
            self._results.current_page, trace_result,
            file_path=self._results.telemetry_info.trace_local_path,
            remote_path=self._results.telemetry_info.trace_remote_path,
            upload_bucket=self._results.telemetry_info.upload_bucket,
            cloud_url=self._results.telemetry_info.trace_remote_url)

        self._results.AddValue(trace_value)
