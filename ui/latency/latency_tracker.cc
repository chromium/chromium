// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_tracker.h"

#include <algorithm>
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/latency/latency_histogram_macros.h"

// Impose some restrictions for tests etc, but also be lenient since some of the
// data come from untrusted sources.
#define DCHECK_AND_RETURN_ON_FAIL(x) \
  DCHECK(x);                         \
  if (!(x))                          \
    return;

namespace ui {
namespace {

std::string LatencySourceEventTypeToInputModalityString(
    ui::SourceEventType type) {
  switch (type) {
    case ui::SourceEventType::WHEEL:
      return "Wheel";
    case ui::SourceEventType::MOUSE:
      return "Mouse";
    case ui::SourceEventType::TOUCH:
    case ui::SourceEventType::INERTIAL:
      return "Touch";
    case ui::SourceEventType::KEY_PRESS:
      return "KeyPress";
    case ui::SourceEventType::TOUCHPAD:
      return "Touchpad";
    case ui::SourceEventType::SCROLLBAR:
      return "Scrollbar";
    default:
      return "";
  }
}

bool IsInertialScroll(const LatencyInfo& latency) {
  return latency.source_event_type() == ui::SourceEventType::INERTIAL;
}

// This UMA metric tracks the time from when the original wheel event is created
// to when the scroll gesture results in final frame swap. All scroll events are
// included in this metric.
void RecordUmaEventLatencyScrollWheelTimeToScrollUpdateSwapBegin2Histogram(
    base::TimeTicks start,
    base::TimeTicks end) {
  CONFIRM_EVENT_TIMES_EXIST(start, end);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Event.Latency.Scroll.Wheel.TimeToScrollUpdateSwapBegin2",
      std::max(static_cast<int64_t>(0), (end - start).InMicroseconds()), 1,
      1000000, 100);
}

LatencyTracker::LatencyInfoProcessor& GetLatencyInfoProcessor() {
  static base::NoDestructor<LatencyTracker::LatencyInfoProcessor> processor;
  return *processor;
}

bool LatencyTraceIdCompare(const LatencyInfo& i, const LatencyInfo& j) {
  return i.trace_id() < j.trace_id();
}

}  // namespace

LatencyTracker::LatencyTracker() = default;
LatencyTracker::~LatencyTracker() = default;

void LatencyTracker::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info) {
  auto& callback = GetLatencyInfoProcessor();
  if (!callback.is_null())
    callback.Run(latency_info);
  // Sort latency_info as they can be in incorrect order.
  std::vector<ui::LatencyInfo> latency_infos(latency_info);
  std::sort(latency_infos.begin(), latency_infos.end(), LatencyTraceIdCompare);
  for (const auto& latency : latency_infos)
    OnGpuSwapBuffersCompleted(latency);
}

void LatencyTracker::OnGpuSwapBuffersCompleted(const LatencyInfo& latency) {
  base::TimeTicks gpu_swap_end_timestamp;
  if (!latency.FindLatency(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
                           &gpu_swap_end_timestamp)) {
    return;
  }

  base::TimeTicks gpu_swap_begin_timestamp;
  bool found_component = latency.FindLatency(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, &gpu_swap_begin_timestamp);
  DCHECK_AND_RETURN_ON_FAIL(found_component);

  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           nullptr)) {
    return;
  }

  ui::SourceEventType source_event_type = latency.source_event_type();
  if (source_event_type == ui::SourceEventType::WHEEL ||
      source_event_type == ui::SourceEventType::MOUSE ||
      source_event_type == ui::SourceEventType::TOUCH ||
      source_event_type == ui::SourceEventType::INERTIAL ||
      source_event_type == ui::SourceEventType::KEY_PRESS ||
      source_event_type == ui::SourceEventType::TOUCHPAD ||
      source_event_type == ui::SourceEventType::SCROLLBAR) {
    ComputeEndToEndLatencyHistograms(gpu_swap_begin_timestamp,
                                     gpu_swap_end_timestamp, latency);
  }
}

void LatencyTracker::ReportUkmScrollLatency(
    const InputMetricEvent& metric_event,
    base::TimeTicks start_timestamp,
    base::TimeTicks time_to_scroll_update_swap_begin_timestamp,
    base::TimeTicks time_to_handled_timestamp,
    bool is_main_thread,
    const ukm::SourceId ukm_source_id) {
  CONFIRM_EVENT_TIMES_EXIST(start_timestamp,
                            time_to_scroll_update_swap_begin_timestamp)
  CONFIRM_EVENT_TIMES_EXIST(start_timestamp, time_to_handled_timestamp)

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (ukm_source_id == ukm::kInvalidSourceId || !ukm_recorder)
    return;

  std::string event_name = "";
  switch (metric_event) {
    case InputMetricEvent::SCROLL_BEGIN_TOUCH:
      event_name = "Event.ScrollBegin.Touch";
      break;
    case InputMetricEvent::SCROLL_UPDATE_TOUCH:
      event_name = "Event.ScrollUpdate.Touch";
      break;
    case InputMetricEvent::SCROLL_BEGIN_WHEEL:
      event_name = "Event.ScrollBegin.Wheel";
      break;
    case InputMetricEvent::SCROLL_UPDATE_WHEEL:
      event_name = "Event.ScrollUpdate.Wheel";
      break;
  }

  ukm::UkmEntryBuilder builder(ukm_source_id, event_name.c_str());
  builder.SetMetric(
      "TimeToScrollUpdateSwapBegin",
      std::max(static_cast<int64_t>(0),
               (time_to_scroll_update_swap_begin_timestamp - start_timestamp)
                   .InMicroseconds()));
  builder.SetMetric(
      "TimeToHandled",
      std::max(static_cast<int64_t>(0),
               (time_to_handled_timestamp - start_timestamp).InMicroseconds()));
  builder.SetMetric("IsMainThread", is_main_thread);
  builder.Record(ukm_recorder);
}

void LatencyTracker::ComputeEndToEndLatencyHistograms(
    base::TimeTicks gpu_swap_begin_timestamp,
    base::TimeTicks gpu_swap_end_timestamp,
    const ui::LatencyInfo& latency) {
  DCHECK_AND_RETURN_ON_FAIL(!latency.coalesced());

  base::TimeTicks original_timestamp;
  std::string scroll_name = "Uninitialized";

  const std::string input_modality =
      LatencySourceEventTypeToInputModalityString(latency.source_event_type());

  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          &original_timestamp)) {
    DCHECK(input_modality == "Wheel" || input_modality == "Touch" ||
           input_modality == "Scrollbar");

    // For inertial scrolling we don't separate the first event from the rest of
    // them.
    scroll_name = IsInertialScroll(latency) ? "ScrollInertial" : "ScrollBegin";

    // This UMA metric tracks the performance of overall scrolling as a high
    // level metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency.ScrollBegin.TimeToScrollUpdateSwapBegin2",
        original_timestamp, gpu_swap_begin_timestamp);

    // This UMA metric tracks the time between the final frame swap for the
    // first scroll event in a sequence and the original timestamp of that
    // scroll event's underlying touch/wheel event.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency." + scroll_name + "." + input_modality +
            ".TimeToScrollUpdateSwapBegin4",
        original_timestamp, gpu_swap_begin_timestamp);

    if (input_modality == "Wheel") {
      RecordUmaEventLatencyScrollWheelTimeToScrollUpdateSwapBegin2Histogram(
          original_timestamp, gpu_swap_begin_timestamp);
    }

  } else if (latency.FindLatency(
                 ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                 &original_timestamp)) {
    DCHECK(input_modality == "Wheel" || input_modality == "Touch" ||
           input_modality == "Scrollbar");
    // For inertial scrolling we don't separate the first event from the rest of
    // them.
    scroll_name = IsInertialScroll(latency) ? "ScrollInertial" : "ScrollUpdate";

    // This UMA metric tracks the performance of overall scrolling as a high
    // level metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency.ScrollUpdate.TimeToScrollUpdateSwapBegin2",
        original_timestamp, gpu_swap_begin_timestamp);

    // This UMA metric tracks the time from when the original touch/wheel event
    // is created to when the scroll gesture results in final frame swap.
    // First scroll events are excluded from this metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency." + scroll_name + "." + input_modality +
            ".TimeToScrollUpdateSwapBegin4",
        original_timestamp, gpu_swap_begin_timestamp);

    if (input_modality == "Wheel") {
      RecordUmaEventLatencyScrollWheelTimeToScrollUpdateSwapBegin2Histogram(
          original_timestamp, gpu_swap_begin_timestamp);
    }
  } else if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                 &original_timestamp)) {
    if (latency.source_event_type() == SourceEventType::KEY_PRESS) {
      UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
          "Event.Latency.EndToEnd.KeyPress", original_timestamp,
          gpu_swap_begin_timestamp);
    } else if (latency.source_event_type() == SourceEventType::MOUSE) {
      UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
          "Event.Latency.EndToEnd.Mouse", original_timestamp,
          gpu_swap_begin_timestamp);
    } else if (latency.source_event_type() == SourceEventType::TOUCHPAD) {
      base::TimeTicks timestamp;
      if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                              &timestamp)) {
        UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_MICROSECONDS(
            "Event.Latency.EventToRender.TouchpadPinch", original_timestamp,
            timestamp);
      }
      {
        // TODO(nburris): Deprecate Event.Latency.EndToEnd.TouchpadPinch in
        // favor of TouchpadPinch2 once we have stable data for that one.
        UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_MICROSECONDS(
            "Event.Latency.EndToEnd.TouchpadPinch", original_timestamp,
            gpu_swap_begin_timestamp);
      }
      UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_1_SECOND_MAX_MICROSECONDS(
          "Event.Latency.EndToEnd.TouchpadPinch2", original_timestamp,
          gpu_swap_begin_timestamp);
    }
    return;
  } else {
    // No original component found.
    return;
  }

  // Record scroll latency metrics.
  DCHECK(scroll_name == "ScrollBegin" || scroll_name == "ScrollUpdate" ||
         (IsInertialScroll(latency) && scroll_name == "ScrollInertial"));

  base::TimeTicks rendering_scheduled_timestamp;
  bool rendering_scheduled_on_main = latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT,
      &rendering_scheduled_timestamp);
  if (!rendering_scheduled_on_main) {
    bool found_component = latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
        &rendering_scheduled_timestamp);
    DCHECK_AND_RETURN_ON_FAIL(found_component);
  }

  if (!IsInertialScroll(latency) && input_modality == "Touch") {
    average_lag_tracker_.AddLatencyInFrame(latency, gpu_swap_begin_timestamp,
                                           scroll_name);
  }

  // Inertial and scrollbar scrolls are excluded from Ukm metrics.
  if ((input_modality == "Touch" && !IsInertialScroll(latency)) ||
      input_modality == "Wheel") {
    InputMetricEvent input_metric_event;
    if (scroll_name == "ScrollBegin") {
      input_metric_event = input_modality == "Touch"
                               ? InputMetricEvent::SCROLL_BEGIN_TOUCH
                               : InputMetricEvent::SCROLL_BEGIN_WHEEL;
    } else {
      DCHECK_EQ(scroll_name, "ScrollUpdate");
      input_metric_event = input_modality == "Touch"
                               ? InputMetricEvent::SCROLL_UPDATE_TOUCH
                               : InputMetricEvent::SCROLL_UPDATE_WHEEL;
    }
    ReportUkmScrollLatency(
        input_metric_event, original_timestamp, gpu_swap_begin_timestamp,
        rendering_scheduled_timestamp, rendering_scheduled_on_main,
        latency.ukm_source_id());
  }

  const std::string thread_name = rendering_scheduled_on_main ? "Main" : "Impl";

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".TimeToHandled2_" + thread_name,
      original_timestamp, rendering_scheduled_timestamp);

  if (input_modality == "Wheel") {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
        "Event.Latency.Scroll.Wheel.TimeToHandled2_" + thread_name,
        original_timestamp, rendering_scheduled_timestamp);
  }

  base::TimeTicks renderer_swap_timestamp;
  bool found_renderer_swap_component =
      latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
                          &renderer_swap_timestamp);

  base::TimeTicks browser_received_swap_timestamp;
  bool found_received_frame_component =
      latency.FindLatency(ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT,
                          &browser_received_swap_timestamp);
  DCHECK_AND_RETURN_ON_FAIL(found_received_frame_component);

  // If we're committing to the active tree, there will never be a renderer
  // swap. In this case, don't record the two histogram values for the periods
  // surrounding the renderer swap. We could assign the total time to one or the
  // other of them, but that would likely skew statistics.
  if (found_renderer_swap_component) {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
        "Event.Latency." + scroll_name + "." + input_modality +
            ".HandledToRendererSwap2_" + thread_name,
        rendering_scheduled_timestamp, renderer_swap_timestamp);

    UMA_HISTOGRAM_SCROLL_LATENCY_SHORT_2(
        "Event.Latency." + scroll_name + "." + input_modality +
            ".RendererSwapToBrowserNotified2",
        renderer_swap_timestamp, browser_received_swap_timestamp);
  }

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(
      "Event.Latency." + scroll_name + "." + input_modality +
          ".BrowserNotifiedToBeforeGpuSwap2",
      browser_received_swap_timestamp, gpu_swap_begin_timestamp);

  UMA_HISTOGRAM_SCROLL_LATENCY_SHORT_2(
      "Event.Latency." + scroll_name + "." + input_modality + ".GpuSwap2",
      gpu_swap_begin_timestamp, gpu_swap_end_timestamp);
}

// static
void LatencyTracker::SetLatencyInfoProcessorForTesting(
    const LatencyInfoProcessor& processor) {
  GetLatencyInfoProcessor() = processor;
}

}  // namespace ui
