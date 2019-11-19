// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/motion_event_buffer.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "ui/events/gesture_detection/motion_event_generic.h"

namespace ui {
namespace {

// Latency added during resampling. A few milliseconds doesn't hurt much but
// reduces the impact of mispredicted touch positions.
const int kResampleLatencyMs = 5;

// Minimum time difference between consecutive samples before attempting to
// resample.
const int kResampleMinDeltaMs = 2;

// Maximum time to predict forward from the last known state, to avoid
// predicting too far into the future.  This time is further bounded by 50% of
// the last time delta.
const int kResampleMaxPredictionMs = 8;

using MotionEventVector = std::vector<std::unique_ptr<MotionEventGeneric>>;

float Lerp(float a, float b, float alpha) {
  return a + alpha * (b - a);
}

bool CanAddSample(const MotionEvent& event0, const MotionEvent& event1) {
  DCHECK_EQ(event0.GetAction(), MotionEvent::Action::MOVE);
  if (event1.GetAction() != MotionEvent::Action::MOVE)
    return false;

  const size_t pointer_count = event0.GetPointerCount();
  if (pointer_count != event1.GetPointerCount())
    return false;

  for (size_t event0_i = 0; event0_i < pointer_count; ++event0_i) {
    const int id = event0.GetPointerId(event0_i);
    const int event1_i = event1.FindPointerIndexOfId(id);
    if (event1_i == -1)
      return false;
    if (event0.GetToolType(event0_i) != event1.GetToolType(event1_i))
      return false;
  }

  return true;
}

bool ShouldResampleTool(MotionEvent::ToolType tool) {
  return tool == MotionEvent::ToolType::UNKNOWN ||
         tool == MotionEvent::ToolType::FINGER;
}

// Splits a chunk of events from the front of the provided |batch| and returns
// it. Requires that |batch| is sorted.
MotionEventVector ConsumeSamplesNoLaterThan(MotionEventVector* batch,
                                            base::TimeTicks time) {
  DCHECK(batch);
  auto first_kept_event = std::partition_point(
      batch->begin(), batch->end(),
      [time](const std::unique_ptr<MotionEventGeneric>& event) {
        return event->GetEventTime() <= time;
      });
  MotionEventVector result(std::make_move_iterator(batch->begin()),
                           std::make_move_iterator(first_kept_event));
  batch->erase(batch->begin(), first_kept_event);
  return result;
}

// Linearly interpolate the pointer position between two MotionEvent samples.
// Only pointers of finger or unknown type will be resampled.
PointerProperties ResamplePointer(const MotionEvent& event0,
                                  const MotionEvent& event1,
                                  size_t event0_pointer_index,
                                  size_t event1_pointer_index,
                                  float alpha) {
  DCHECK_EQ(event0.GetPointerId(event0_pointer_index),
            event1.GetPointerId(event1_pointer_index));
  // If the tool should not be resampled, use the latest event in the valid
  // horizon (i.e., the event no later than the time interpolated by alpha).
  if (!ShouldResampleTool(event0.GetToolType(event0_pointer_index))) {
    if (alpha > 1)
      return PointerProperties(event1, event1_pointer_index);
    else
      return PointerProperties(event0, event0_pointer_index);
  }

  PointerProperties p(event0, event0_pointer_index);
  p.x = Lerp(p.x, event1.GetX(event1_pointer_index), alpha);
  p.y = Lerp(p.y, event1.GetY(event1_pointer_index), alpha);
  p.raw_x = Lerp(p.raw_x, event1.GetRawX(event1_pointer_index), alpha);
  p.raw_y = Lerp(p.raw_y, event1.GetRawY(event1_pointer_index), alpha);
  return p;
}

// Linearly interpolate the pointers between two event samples using the
// provided |resample_time|.
std::unique_ptr<MotionEventGeneric> ResampleMotionEvent(
    const MotionEvent& event0,
    const MotionEvent& event1,
    base::TimeTicks resample_time) {
  DCHECK_EQ(MotionEvent::Action::MOVE, event0.GetAction());
  DCHECK_EQ(event0.GetPointerCount(), event1.GetPointerCount());

  const base::TimeTicks time0 = event0.GetEventTime();
  const base::TimeTicks time1 = event1.GetEventTime();
  DCHECK(time0 < time1);
  DCHECK(time0 <= resample_time);

  const float alpha = (resample_time - time0).InMillisecondsF() /
                      (time1 - time0).InMillisecondsF();

  std::unique_ptr<MotionEventGeneric> event;
  const size_t pointer_count = event0.GetPointerCount();
  DCHECK_EQ(pointer_count, event1.GetPointerCount());
  for (size_t event0_i = 0; event0_i < pointer_count; ++event0_i) {
    int event1_i = event1.FindPointerIndexOfId(event0.GetPointerId(event0_i));
    DCHECK_NE(event1_i, -1);
    PointerProperties pointer = ResamplePointer(
        event0, event1, event0_i, static_cast<size_t>(event1_i), alpha);

    if (event0_i == 0) {
      event = std::make_unique<MotionEventGeneric>(MotionEvent::Action::MOVE,
                                                   resample_time, pointer);
    } else {
      event->PushPointer(pointer);
    }
  }

  DCHECK(event);
  event->set_button_state(event0.GetButtonState());
  return event;
}

// Synthesize a compound MotionEventGeneric event from a sequence of events.
// Events must be in non-decreasing (time) order.
std::unique_ptr<MotionEventGeneric> ConsumeSamples(MotionEventVector events) {
  DCHECK(!events.empty());
  std::unique_ptr<MotionEventGeneric> event = std::move(events.back());
  events.pop_back();
  for (auto& historic_event : events)
    event->PushHistoricalEvent(std::move(historic_event));
  return event;
}

// Consume a series of event samples, attempting to synthesize a new, synthetic
// event if the samples and sample time meet certain interpolation/extrapolation
// conditions. If such conditions are met, the provided samples will be added
// to the synthetic event's history, otherwise, the samples will be used to
// generate a basic, compound event.
// TODO(jdduke): Revisit resampling to handle cases where alternating frames
// are resampled or resampling is otherwise inconsistent, e.g., a 90hz input
// and 60hz frame signal could phase-align such that even frames yield an
// extrapolated event and odd frames are not resampled, crbug.com/399381.
std::unique_ptr<MotionEventGeneric> ConsumeSamplesAndTryResampling(
    base::TimeTicks resample_time,
    MotionEventVector events,
    const MotionEvent* next) {
  const ui::MotionEvent* event0 = nullptr;
  const ui::MotionEvent* event1 = nullptr;
  if (next) {
    DCHECK(resample_time < next->GetEventTime());
    // Interpolate between current sample and future sample.
    event0 = events.back().get();
    event1 = next;
  } else if (events.size() >= 2) {
    // Extrapolate future sample using current sample and past sample.
    event0 = events[events.size() - 2].get();
    event1 = events[events.size() - 1].get();

    const base::TimeTicks time1 = event1->GetEventTime();
    base::TimeTicks max_predict =
        time1 +
        std::min((event1->GetEventTime() - event0->GetEventTime()) / 2,
                 base::TimeDelta::FromMilliseconds(kResampleMaxPredictionMs));
    if (resample_time > max_predict) {
      TRACE_EVENT_INSTANT2("input",
                           "MotionEventBuffer::TryResample prediction adjust",
                           TRACE_EVENT_SCOPE_THREAD,
                           "original(ms)",
                           (resample_time - time1).InMilliseconds(),
                           "adjusted(ms)",
                           (max_predict - time1).InMilliseconds());
      resample_time = max_predict;
    }
  } else {
    TRACE_EVENT_INSTANT0("input",
                         "MotionEventBuffer::TryResample insufficient data",
                         TRACE_EVENT_SCOPE_THREAD);
    return ConsumeSamples(std::move(events));
  }

  DCHECK(event0);
  DCHECK(event1);
  const base::TimeTicks time0 = event0->GetEventTime();
  const base::TimeTicks time1 = event1->GetEventTime();
  base::TimeDelta delta = time1 - time0;
  if (delta < base::TimeDelta::FromMilliseconds(kResampleMinDeltaMs)) {
    TRACE_EVENT_INSTANT1("input",
                         "MotionEventBuffer::TryResample failure",
                         TRACE_EVENT_SCOPE_THREAD,
                         "event_delta_too_small(ms)",
                         delta.InMilliseconds());
    return ConsumeSamples(std::move(events));
  }

  std::unique_ptr<MotionEventGeneric> resampled_event =
      ResampleMotionEvent(*event0, *event1, resample_time);
  for (auto& historic_event : events)
    resampled_event->PushHistoricalEvent(std::move(historic_event));
  return resampled_event;
}

}  // namespace

MotionEventBuffer::MotionEventBuffer(MotionEventBufferClient* client,
                                     bool enable_resampling)
    : client_(client), resample_(enable_resampling) {
}

MotionEventBuffer::~MotionEventBuffer() {
}

void MotionEventBuffer::OnMotionEvent(const MotionEvent& event) {
  DCHECK_EQ(0U, event.GetHistorySize());
  if (event.GetAction() != MotionEvent::Action::MOVE) {
    last_extrapolated_event_time_ = base::TimeTicks();
    if (!buffered_events_.empty())
      FlushWithoutResampling(std::move(buffered_events_));
    client_->ForwardMotionEvent(event);
    return;
  }

  // Guard against events that are *older* than the last one that may have been
  // artificially synthesized.
  if (!last_extrapolated_event_time_.is_null()) {
    DCHECK(buffered_events_.empty());
    if (event.GetEventTime() < last_extrapolated_event_time_)
      return;
    last_extrapolated_event_time_ = base::TimeTicks();
  }

  std::unique_ptr<MotionEventGeneric> clone =
      MotionEventGeneric::CloneEvent(event);
  if (buffered_events_.empty()) {
    buffered_events_.push_back(std::move(clone));
    client_->SetNeedsFlush();
    return;
  }

  if (CanAddSample(*buffered_events_.front(), *clone)) {
    // Ensure that buffered_events_ is ordered.
    DCHECK(buffered_events_.back()->GetEventTime() <= clone->GetEventTime());
  } else {
    FlushWithoutResampling(std::move(buffered_events_));
  }

  buffered_events_.push_back(std::move(clone));
  // No need to request another flush as the first event will have requested it.
}

void MotionEventBuffer::Flush(base::TimeTicks frame_time) {
  if (buffered_events_.empty())
    return;

  // Shifting the sample time back slightly minimizes the potential for
  // misprediction when extrapolating events.
  if (resample_)
    frame_time -= base::TimeDelta::FromMilliseconds(kResampleLatencyMs);

  // TODO(jdduke): Use a persistent MotionEventVector vector for temporary
  // storage.
  MotionEventVector events =
      ConsumeSamplesNoLaterThan(&buffered_events_, frame_time);
  if (events.empty()) {
    DCHECK(!buffered_events_.empty());
    client_->SetNeedsFlush();
    return;
  }

  if (!resample_ || (events.size() == 1 && buffered_events_.empty())) {
    FlushWithoutResampling(std::move(events));
    if (!buffered_events_.empty())
      client_->SetNeedsFlush();
    return;
  }

  FlushWithResampling(std::move(events), frame_time);
}

void MotionEventBuffer::FlushWithResampling(MotionEventVector events,
                                            base::TimeTicks resample_time) {
  DCHECK(!events.empty());
  base::TimeTicks original_event_time = events.back()->GetEventTime();
  const MotionEvent* next_event =
      !buffered_events_.empty() ? buffered_events_.front().get() : nullptr;

  std::unique_ptr<MotionEventGeneric> resampled_event =
      ConsumeSamplesAndTryResampling(resample_time, std::move(events),
                                     next_event);
  DCHECK(resampled_event);

  // Log the extrapolated event time, guarding against subsequently queued
  // events that might have an earlier timestamp.
  if (!next_event && resampled_event->GetEventTime() > original_event_time) {
    last_extrapolated_event_time_ = resampled_event->GetEventTime();
  } else {
    last_extrapolated_event_time_ = base::TimeTicks();
  }

  client_->ForwardMotionEvent(*resampled_event);
  if (!buffered_events_.empty())
    client_->SetNeedsFlush();
}

void MotionEventBuffer::FlushWithoutResampling(MotionEventVector events) {
  last_extrapolated_event_time_ = base::TimeTicks();
  if (events.empty())
    return;

  client_->ForwardMotionEvent(*ConsumeSamples(std::move(events)));
}

}  // namespace ui
