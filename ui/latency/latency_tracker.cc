// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_tracker.h"

#include <algorithm>
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
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

enum Jank : int {
  kNonJanky = 0,
  kJanky,
};

void EmitScrollUpdateTime(base::TimeDelta dur, bool janky) {
  int count = dur.InMilliseconds();
  if (count <= 0) {
    // Histograms aren't allowed to add zero counts, this could happen
    // especially in tests when the clock hasn't advanced enough for a
    // microsecond to have passed.
    return;
  }
  static auto* histogram = base::BooleanHistogram::FactoryGet(
      "Event.Jank.ScrollUpdate.TotalJankyAndNonJankyDuration2",
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(janky ? kJanky : kNonJanky, count);
}

}  // namespace

LatencyTracker::LatencyTracker() = default;
LatencyTracker::~LatencyTracker() = default;

void LatencyTracker::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    bool top_controls_visible_height_changed) {
  // ReportJankyFrame has to process latency infos in increasing trace_id
  // order, so it can compare the current frame to previous one. Therefore, the
  // vector is sorted here before passing it down the call chain.
  std::sort(latency_info.begin(), latency_info.end(),
            [](const LatencyInfo& x, const LatencyInfo& y) {
              return x.trace_id() < y.trace_id();
            });

  for (const auto& latency : latency_info) {
    base::TimeTicks gpu_swap_end_timestamp;
    if (!latency.FindLatency(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
                             &gpu_swap_end_timestamp)) {
      continue;
    }

    base::TimeTicks gpu_swap_begin_timestamp;
    bool found_component = latency.FindLatency(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, &gpu_swap_begin_timestamp);
    DCHECK_AND_RETURN_ON_FAIL(found_component);

    if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                             nullptr)) {
      continue;
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
                                       gpu_swap_end_timestamp, latency,
                                       top_controls_visible_height_changed);
    }
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

// Checking whether the update event |tested_frames| length (measured in frames)
// is janky compared to another |basis_frames| (either previous or next). Update
// is deemed janky when it's half of a frame longer than a neighbouring update.
//
// A small number is added to 0.5 in order to make sure that the comparison does
// not filter out ratios that are precisely 0.5, which can fall a little above
// or below exact value due to inherent inaccuracy of operations with
// floating-point numbers. Value 1e-9 have been chosen as follows: the ratio has
// less than nanosecond precision in numerator and VSync interval in
// denominator. Assuming refresh rate more than 1 FPS (and therefore VSync
// interval less than a second), this ratio should increase with increments more
// than minimal value in numerator (1ns) divided by maximum value in
// denominator, giving 1e-9.
static bool IsJankyComparison(double tested_frames, double basis_frames) {
  return tested_frames > basis_frames + 0.5 + 1e-9;
}

void LatencyTracker::ReportJankyFrame(base::TimeTicks original_timestamp,
                                      base::TimeTicks gpu_swap_end_timestamp,
                                      const ui::LatencyInfo& latency,
                                      bool first_frame) {
  CONFIRM_EVENT_TIMES_EXIST(original_timestamp, gpu_swap_end_timestamp);
  base::TimeDelta dur = gpu_swap_end_timestamp - original_timestamp;

  if (first_frame) {
    if (jank_state_.total_update_events_ > 0) {
      // If we have some data from previous scroll, report it to UMA.
      UMA_HISTOGRAM_MEDIUM_TIMES("Event.Latency.ScrollUpdate.TotalDuration",
                                 jank_state_.total_update_duration_);
      UMA_HISTOGRAM_MEDIUM_TIMES("Event.Latency.ScrollUpdate.JankyDuration",
                                 jank_state_.janky_update_duration_);

      UMA_HISTOGRAM_COUNTS_10000("Event.Latency.ScrollUpdate.TotalEvents",
                                 jank_state_.total_update_events_);
      UMA_HISTOGRAM_COUNTS_10000("Event.Latency.ScrollUpdate.JankyEvents",
                                 jank_state_.janky_update_events_);

      if (!jank_state_.total_update_duration_.is_zero()) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Event.Latency.ScrollUpdate.JankyDurationPercentage",
            static_cast<int>(100 * (jank_state_.janky_update_duration_ /
                                    jank_state_.total_update_duration_)));
      }
    }

    jank_state_ = JankTrackerState{};
  }

  jank_state_.total_update_events_++;
  jank_state_.total_update_duration_ += dur;

  // When processing first frame in a scroll, we do not have any other frames to
  // compare it to, and thus no way to detect the jank.
  if (!first_frame) {
    // TODO(185884172): Investigate using proper vsync interval.

    // Assuming 60fps, each frame is rendered in (1/60) of a second.
    // To see how many of those intervals fit into the real frame timing,
    // we divide it on 1/60 which is the same thing as multiplying by 60.
    double frames_taken = dur.InSecondsF() * 60;
    double prev_frames_taken = jank_state_.prev_duration_.InSecondsF() * 60;

    // For each GestureScroll update, we would like to report whether it was
    // janky. However, in order to do that, we need to compare it both to the
    // previous as well as to the next event. This condition means that no jank
    // was reported for the previous frame (as compared to the one before that),
    // so we need to compare it to the current one and report whether it's
    // janky:
    if (!jank_state_.prev_scroll_update_reported_) {
      // The information about previous GestureScrollUpdate was not reported:
      // check whether it's janky by comparing to the current frame and report.
      bool janky = false;
      if (IsJankyComparison(prev_frames_taken, frames_taken)) {
        janky = true;
        jank_state_.janky_update_events_++;
        jank_state_.janky_update_duration_ += jank_state_.prev_duration_;
      }
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.ScrollJank", janky);
      EmitScrollUpdateTime(jank_state_.prev_duration_, janky);
    }

    // The current GestureScrollUpdate is janky compared to the previous one.
    if (IsJankyComparison(frames_taken, prev_frames_taken)) {
      UMA_HISTOGRAM_BOOLEAN("Event.Latency.ScrollJank", true);
      EmitScrollUpdateTime(dur, true);
      jank_state_.janky_update_events_++;
      jank_state_.janky_update_duration_ += dur;

      // Since we have reported the current event as janky, there is no need to
      // report anything about it on the next iteration, as we would like to
      // report every GestureScrollUpdate only once.
      jank_state_.prev_scroll_update_reported_ = true;
    } else {
      // We do not have enough information to report whether the current event
      // is janky, and need to compare it to the next one before reporting
      // anything about it.
      jank_state_.prev_scroll_update_reported_ = false;
    }
  }

  jank_state_.prev_duration_ = dur;
}

void LatencyTracker::ComputeEndToEndLatencyHistograms(
    base::TimeTicks gpu_swap_begin_timestamp,
    base::TimeTicks gpu_swap_end_timestamp,
    const ui::LatencyInfo& latency,
    bool top_controls_visible_height_changed) {
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
    ReportJankyFrame(original_timestamp, gpu_swap_end_timestamp, latency, true);

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
    std::string metric_name =
        base::StrCat({"Event.Latency.", scroll_name, ".", input_modality,
                      ".TimeToScrollUpdateSwapBegin4"});
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        metric_name, original_timestamp, gpu_swap_begin_timestamp);

    // Report the latency metric separately for the scrolls that caused the
    // top-controls to scroll and the ones that didn't.
    if (top_controls_visible_height_changed)
      base::StrAppend(&metric_name, {".TopControlsMoved"});
    else
      base::StrAppend(&metric_name, {".NoTopControlsMoved"});
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        metric_name, original_timestamp, gpu_swap_begin_timestamp);

  } else if (latency.FindLatency(
                 ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                 &original_timestamp)) {
    DCHECK(input_modality == "Wheel" || input_modality == "Touch" ||
           input_modality == "Scrollbar");
    ReportJankyFrame(original_timestamp, gpu_swap_end_timestamp, latency,
                     false);

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
    std::string metric_name =
        base::StrCat({"Event.Latency.", scroll_name, ".", input_modality,
                      ".TimeToScrollUpdateSwapBegin4"});
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        metric_name, original_timestamp, gpu_swap_begin_timestamp);

    // Also report the latency metric separately for the scrolls that caused the
    // top-controls to scroll and the ones that didn't.
    if (top_controls_visible_height_changed)
      base::StrAppend(&metric_name, {".TopControlsMoved"});
    else
      base::StrAppend(&metric_name, {".NoTopControlsMoved"});
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        metric_name, original_timestamp, gpu_swap_begin_timestamp);

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

}  // namespace ui
