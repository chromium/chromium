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
#define DCHECK_AND_CONTINUE_ON_FAIL(x) \
  DCHECK(x);                           \
  if (!(x))                            \
    continue;

namespace ui {

LatencyTracker::LatencyTracker() = default;
LatencyTracker::~LatencyTracker() = default;

void LatencyTracker::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info) {
  // We only calculate UKMs now.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder) {
    return;
  }

  for (const auto& latency : latency_info) {
    if (latency.ukm_source_id() == ukm::kInvalidSourceId) {
      continue;
    }
    // Inertial and scrollbar scrolls are excluded from Ukm metrics.
    ui::SourceEventType source_event_type = latency.source_event_type();
    if (!(source_event_type == ui::SourceEventType::WHEEL ||
          source_event_type == ui::SourceEventType::TOUCH)) {
      continue;
    }

    if (!latency.FindLatency(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
                             nullptr)) {
      continue;
    }

    base::TimeTicks gpu_swap_begin_timestamp;
    bool found_component = latency.FindLatency(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, &gpu_swap_begin_timestamp);
    DCHECK_AND_CONTINUE_ON_FAIL(found_component);

    if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                             nullptr)) {
      continue;
    }

    DCHECK_AND_CONTINUE_ON_FAIL(!latency.coalesced());

    base::TimeTicks original_timestamp;
    InputMetricEvent input_metric_event;
    if (latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
            &original_timestamp)) {
      input_metric_event = source_event_type == ui::SourceEventType::TOUCH
                               ? InputMetricEvent::SCROLL_BEGIN_TOUCH
                               : InputMetricEvent::SCROLL_BEGIN_WHEEL;
    } else if (latency.FindLatency(
                   ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                   &original_timestamp)) {
      input_metric_event = source_event_type == ui::SourceEventType::TOUCH
                               ? InputMetricEvent::SCROLL_UPDATE_TOUCH
                               : InputMetricEvent::SCROLL_UPDATE_WHEEL;
    } else {
      continue;
    }

    base::TimeTicks rendering_scheduled_timestamp;
    bool rendering_scheduled_on_main = latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT,
        &rendering_scheduled_timestamp);
    if (!rendering_scheduled_on_main) {
      bool found_component_impl = latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
          &rendering_scheduled_timestamp);
      DCHECK_AND_CONTINUE_ON_FAIL(found_component_impl);
    }

    ReportUkmScrollLatency(
        input_metric_event, original_timestamp, gpu_swap_begin_timestamp,
        rendering_scheduled_timestamp, rendering_scheduled_on_main,
        latency.ukm_source_id());
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

}  // namespace ui
