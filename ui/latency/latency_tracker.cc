// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_tracker.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
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

namespace ui {
namespace {

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
std::string_view LatencyTracker::ToString(ScrollInputModality modality) {
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
std::string_view LatencyTracker::ToString(ScrollType type) {
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
    std::string_view suffix,
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
      NOTREACHED_IN_MIGRATION();
  }
  return ScrollInputModality::kLastValue;
}

LatencyTracker::LatencyTracker() = default;
LatencyTracker::~LatencyTracker() = default;

void LatencyTracker::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info) {
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
                                       gpu_swap_end_timestamp, latency);
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
    const ui::LatencyInfo& latency) {
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
    EmitLatencyHistograms(gpu_swap_begin_timestamp, gpu_swap_end_timestamp,
                          original_timestamp, latency, scroll_type,
                          input_modality);
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
