// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_tracker.h"

#include <algorithm>
#include <cstdint>
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/latency/jank_tracker_for_experiments_buildflags.h"
#include "ui/latency/janky_duration_tracker.h"

// Impose some restrictions for tests etc, but also be lenient since some of the
// data come from untrusted sources.
#define DCHECK_AND_RETURN_ON_FAIL(x) \
  DCHECK(x);                         \
  if (!(x))                          \
    return;

// Event latency that is mostly under 5 seconds. We should only use 100 buckets
// when needed.
#define UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(name, latency) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, latency.InMicroseconds(), 1,              \
                              base::Seconds(5).InMicroseconds(), 100);

#define UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(     \
    suffix, scroll_type, input_modality, latency)                         \
  STATIC_HISTOGRAM_POINTER_GROUP(                                         \
      GetHistogramName(suffix, scroll_type, input_modality),              \
      GetHistogramIndex(scroll_type, input_modality), kMaxHistogramIndex, \
      Add(latency.InMicroseconds()),                                      \
      base::Histogram::FactoryGet(                                        \
          GetHistogramName(suffix, scroll_type, input_modality), 1,       \
          base::Seconds(5).InMicroseconds(), 100,                         \
          base::HistogramBase::kUmaTargetedHistogramFlag));

// Event latency that is mostly under 100ms. We should only use 100 buckets
// when needed. This drops reports on clients with low-resolution clocks.
#define UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_MICROSECONDS(name, latency) \
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                             \
      name, latency, base::Microseconds(1), base::Milliseconds(100), 100);

// Deprecated, use UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_MICROSECONDS instead.
// Event latency that is mostly under 1 second. We should only use 100 buckets
// when needed.
#define UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(name,    \
                                                                 latency) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, latency.InMicroseconds(), 1,          \
                              base::Seconds(1).InMicroseconds(), 100);

// Event latency that is mostly under 1 second. We should only use 100 buckets
// when needed. This drops reports on clients with low-resolution clocks.
#define UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_1_SECOND_MAX_MICROSECONDS(name,    \
                                                                     latency) \
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                                    \
      name, latency, base::Microseconds(1), base::Seconds(1), 100);

// Long touch/wheel scroll latency component that is mostly under 200ms.
#define UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2(name, latency)            \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, latency.InMicroseconds(),         \
                              base::Milliseconds(1).InMicroseconds(), \
                              base::Milliseconds(200).InMicroseconds(), 50);

#define UMA_HISTOGRAM_SCROLL_LATENCY_LONG_2_GROUP(suffix, scroll_type,     \
                                                  input_modality, latency) \
  STATIC_HISTOGRAM_POINTER_GROUP(                                          \
      GetHistogramName(suffix, scroll_type, input_modality),               \
      GetHistogramIndex(scroll_type, input_modality), kMaxHistogramIndex,  \
      Add(latency.InMicroseconds()),                                       \
      base::Histogram::FactoryGet(                                         \
          GetHistogramName(suffix, scroll_type, input_modality),           \
          base::Milliseconds(1).InMicroseconds(),                          \
          base::Milliseconds(200).InMicroseconds(), 50,                    \
          base::HistogramBase::kUmaTargetedHistogramFlag));

// Short touch/wheel scroll latency component that is mostly under 50ms.
#define UMA_HISTOGRAM_SCROLL_LATENCY_SHORT_2_GROUP(suffix, scroll_type,     \
                                                   input_modality, latency) \
  STATIC_HISTOGRAM_POINTER_GROUP(                                           \
      GetHistogramName(suffix, scroll_type, input_modality),                \
      GetHistogramIndex(scroll_type, input_modality), kMaxHistogramIndex,   \
      Add(latency.InMicroseconds()),                                        \
      base::Histogram::FactoryGet(                                          \
          GetHistogramName(suffix, scroll_type, input_modality), 1,         \
          base::Milliseconds(50).InMicroseconds(), 50,                      \
          base::HistogramBase::kUmaTargetedHistogramFlag));

namespace ui {
namespace {

base::TimeDelta ComputeLatency(base::TimeTicks start, base::TimeTicks end) {
  DCHECK(!start.is_null());
  DCHECK(!end.is_null());
  base::TimeDelta latency = end - start;
  if (latency.is_negative()) {
    return base::Milliseconds(0);
  }
  return latency;
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
#if BUILDFLAG(JANK_TRACKER_FOR_EXPERIMENTS)
  AdvanceJankyDurationForBenchmarking(janky, count);
#endif
}

}  // namespace

// static
base::StringPiece LatencyTracker::ToString(ScrollInputModality modality) {
  switch (modality) {
    case ScrollInputModality::kWheel:
      return "Wheel";
    case ScrollInputModality::kTouch:
      return "Touch";
    case ScrollInputModality::kScrollbar:
      return "Scrollbar";
  }
}

// static
base::StringPiece LatencyTracker::ToString(ScrollType type) {
  switch (type) {
    case ScrollType::kBegin:
      return "ScrollBegin";
    case ScrollType::kUpdate:
      return "ScrollUpdate";
    case ScrollType::kInertial:
      return "ScrollInertial";
  }
}

// static
int LatencyTracker::GetHistogramIndex(ScrollType scroll_type,
                                      ScrollInputModality input_modality) {
  return static_cast<int>(scroll_type) +
         (static_cast<int>(ScrollType::kLastValue) + 1) *
             static_cast<int>(input_modality);
}

// static
std::string LatencyTracker::GetHistogramName(
    base::StringPiece suffix,
    ScrollType scroll_type,
    ScrollInputModality input_modality) {
  return base::StrCat({"Event.Latency.", ToString(scroll_type), ".",
                       ToString(input_modality), ".", suffix});
}

// static
LatencyTracker::ScrollInputModality LatencyTracker::ToScrollInputModality(
    ui::SourceEventType type) {
  switch (type) {
    case ui::SourceEventType::WHEEL:
      return ScrollInputModality::kWheel;
    case ui::SourceEventType::TOUCH:
    case ui::SourceEventType::INERTIAL:
      return ScrollInputModality::kTouch;
    case ui::SourceEventType::SCROLLBAR:
      return ScrollInputModality::kScrollbar;
    default:
      NOTREACHED();
  }
  return ScrollInputModality::kLastValue;
}

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
  DCHECK(!start_timestamp.is_null());
  DCHECK(!time_to_scroll_update_swap_begin_timestamp.is_null());
  DCHECK(!time_to_handled_timestamp.is_null());

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
  DCHECK(!original_timestamp.is_null());
  DCHECK(!gpu_swap_end_timestamp.is_null());
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

  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          &original_timestamp)) {
    ReportJankyFrame(original_timestamp, gpu_swap_end_timestamp, latency, true);

    ScrollInputModality input_modality =
        ToScrollInputModality(latency.source_event_type());
    // For inertial scrolling we don't separate the first event from the rest of
    // them.
    ScrollType scroll_type =
        IsInertialScroll(latency) ? ScrollType::kInertial : ScrollType::kBegin;

    // This UMA metric tracks the performance of overall scrolling as a high
    // level metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency.ScrollBegin.TimeToScrollUpdateSwapBegin2",
        ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));

    // This UMA metric tracks the time between the final frame swap for the
    // first scroll event in a sequence and the original timestamp of that
    // scroll event's underlying touch/wheel event.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
        "TimeToScrollUpdateSwapBegin4", scroll_type, input_modality,
        ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));

    // Report the latency metric separately for the scrolls that caused the
    // top-controls to scroll and the ones that didn't.
    if (top_controls_visible_height_changed) {
      UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
          "TimeToScrollUpdateSwapBegin4.TopControlsMoved", scroll_type,
          input_modality,
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    } else {
      UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
          "TimeToScrollUpdateSwapBegin4.NoTopControlsMoved", scroll_type,
          input_modality,
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    }

    EmitLatencyHistograms(gpu_swap_begin_timestamp, gpu_swap_end_timestamp,
                          original_timestamp, latency, scroll_type,
                          input_modality);

  } else if (latency.FindLatency(
                 ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                 &original_timestamp)) {
    ReportJankyFrame(original_timestamp, gpu_swap_end_timestamp, latency,
                     false);

    ScrollInputModality input_modality =
        ToScrollInputModality(latency.source_event_type());
    // For inertial scrolling we don't separate the first event from the rest of
    // them.
    ScrollType scroll_type =
        IsInertialScroll(latency) ? ScrollType::kInertial : ScrollType::kUpdate;

    // This UMA metric tracks the performance of overall scrolling as a high
    // level metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS(
        "Event.Latency.ScrollUpdate.TimeToScrollUpdateSwapBegin2",
        ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));

    // This UMA metric tracks the time from when the original touch/wheel event
    // is created to when the scroll gesture results in final frame swap.
    // First scroll events are excluded from this metric.
    UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
        "TimeToScrollUpdateSwapBegin4", scroll_type, input_modality,
        ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));

    // Also report the latency metric separately for the scrolls that caused the
    // top-controls to scroll and the ones that didn't.
    if (top_controls_visible_height_changed) {
      UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
          "TimeToScrollUpdateSwapBegin4.TopControlsMoved", scroll_type,
          input_modality,
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    } else {
      UMA_HISTOGRAM_INPUT_LATENCY_5_SECONDS_MAX_MICROSECONDS_GROUP(
          "TimeToScrollUpdateSwapBegin4.NoTopControlsMoved", scroll_type,
          input_modality,
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    }

    EmitLatencyHistograms(gpu_swap_begin_timestamp, gpu_swap_end_timestamp,
                          original_timestamp, latency, scroll_type,
                          input_modality);

  } else if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                 &original_timestamp)) {
    if (latency.source_event_type() == SourceEventType::KEY_PRESS) {
      UMA_HISTOGRAM_INPUT_LATENCY_HIGH_RESOLUTION_MICROSECONDS(
          "Event.Latency.EndToEnd.KeyPress",
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    } else if (latency.source_event_type() == SourceEventType::TOUCHPAD) {
      UMA_HISTOGRAM_INPUT_LATENCY_CUSTOM_1_SECOND_MAX_MICROSECONDS(
          "Event.Latency.EndToEnd.TouchpadPinch2",
          ComputeLatency(original_timestamp, gpu_swap_begin_timestamp));
    }
  }
}

void LatencyTracker::EmitLatencyHistograms(
    base::TimeTicks gpu_swap_begin_timestamp,
    base::TimeTicks gpu_swap_end_timestamp,
    base::TimeTicks original_timestamp,
    const ui::LatencyInfo& latency,
    ScrollType scroll_type,
    ScrollInputModality input_modality) {
  DCHECK(!IsInertialScroll(latency) || scroll_type == ScrollType::kInertial);

  // Inertial and scrollbar scrolls are excluded from Ukm metrics.
  if (!((input_modality == ScrollInputModality::kTouch &&
         !IsInertialScroll(latency)) ||
        input_modality == ScrollInputModality::kWheel)) {
    return;
  }

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

    InputMetricEvent input_metric_event;
    if (scroll_type == ScrollType::kBegin) {
      input_metric_event = input_modality == ScrollInputModality::kTouch
                               ? InputMetricEvent::SCROLL_BEGIN_TOUCH
                               : InputMetricEvent::SCROLL_BEGIN_WHEEL;
    } else {
      DCHECK_EQ(scroll_type, ScrollType::kUpdate);
      input_metric_event = input_modality == ScrollInputModality::kTouch
                               ? InputMetricEvent::SCROLL_UPDATE_TOUCH
                               : InputMetricEvent::SCROLL_UPDATE_WHEEL;
    }
    ReportUkmScrollLatency(
        input_metric_event, original_timestamp, gpu_swap_begin_timestamp,
        rendering_scheduled_timestamp, rendering_scheduled_on_main,
        latency.ukm_source_id());
}

}  // namespace ui
