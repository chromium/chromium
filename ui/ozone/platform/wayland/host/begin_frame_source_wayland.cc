// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/begin_frame_source_wayland.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"

namespace ui {

BeginFrameSourceWayland::BeginFrameSourceWayland(
    PlatformWindow* window,
    WaylandFrameManager* frame_manager)
    : frame_manager_(frame_manager), window_(window) {
  DCHECK(frame_manager_);
  SetBeginFrameSourceExtension(window, this);
  frame_manager_->AddFrameTimingObserver(this);
}

BeginFrameSourceWayland::~BeginFrameSourceWayland() {
  frame_manager_->RemoveFrameTimingObserver(this);
  SetBeginFrameSourceExtension(window_, nullptr);
}

void BeginFrameSourceWayland::Reset() {
  needs_begin_frame_ = false;
  frame_in_flight_ = false;
  ready_to_issue_begin_frame_ = false;
  last_frame_deadline_time_ = base::TimeTicks();
  last_sent_vsync_interval_ = base::TimeDelta();
  frame_callback_timeout_timer_.Stop();
  deferred_issue_begin_frame_timer_.Stop();
}

void BeginFrameSourceWayland::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void BeginFrameSourceWayland::SetNeedsBeginFrame(bool needs) {
  TRACE_EVENT1("wayland", "BeginFrameSourceWayland::SetNeedsBeginFrame",
               "needs_begin_frames", needs);
  if (needs_begin_frame_ == needs) {
    return;
  }

  needs_begin_frame_ = needs;

  if (needs_begin_frame_) {
    if (!frame_in_flight_) {
      ready_to_issue_begin_frame_ = true;
      MaybeIssueBeginFrame();
    }
  } else {
    ready_to_issue_begin_frame_ = false;
    frame_callback_timeout_timer_.Stop();
    deferred_issue_begin_frame_timer_.Stop();
  }
}

void BeginFrameSourceWayland::SetPreferredInterval(base::TimeDelta interval) {
  TRACE_EVENT1("wayland", "BeginFrameSourceWayland::SetPreferredInterval",
               "interval_us", interval.InMicroseconds());
  if (!interval.is_zero()) {
    DVLOG(1) << "SetPreferredInterval: preferred interval updated to "
             << interval.InMillisecondsF() << "ms";
    preferred_interval_ = interval;
  }
}

// static
base::TimeDelta BeginFrameSourceWayland::ComputeEffectiveInterval(
    base::TimeDelta preferred_interval,
    base::TimeDelta vsync_interval) {
  if (preferred_interval.is_zero() || vsync_interval.is_zero()) {
    return vsync_interval;
  }
  // Matches the vsync subsampling in
  // ExternalBeginFrameSourceMac::SetPreferredInterval: pick the largest whole
  // multiple of the vsync that doesn't exceed the (capped) preferred
  // interval, allowing for a small margin of error for similar intervals.
  constexpr base::TimeDelta kDeltaAlmostEqual = base::Microseconds(10);
  const base::TimeDelta bounded_interval =
      std::min(preferred_interval, kMaxEffectiveInterval);
  const int64_t factor = std::max<int64_t>(
      1, bounded_interval.IntDiv(vsync_interval - kDeltaAlmostEqual));
  return factor * vsync_interval;
}

base::TimeDelta BeginFrameSourceWayland::GetEffectiveInterval() const {
  return ComputeEffectiveInterval(preferred_interval_, vsync_interval_);
}

void BeginFrameSourceWayland::OnFrameCallback(base::TimeTicks callback_time) {
  TRACE_EVENT1("wayland", "BeginFrameSourceWayland::OnFrameCallback",
               "callback_time", callback_time);
  if (frame_callback_timeout_timer_.IsRunning()) {
    frame_callback_timeout_timer_.Stop();
  }
  ready_to_issue_begin_frame_ = true;

  if (!frame_in_flight_) {
    MaybeIssueBeginFrame();
  }
}

void BeginFrameSourceWayland::OnPresentationFeedback(
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT2("wayland", "BeginFrameSourceWayland::OnPresentationFeedback",
               "interval_us", feedback.interval.InMicroseconds(), "failed",
               feedback.failed());
  if (feedback.failed()) {
    return;
  }

  const base::TimeDelta previous_vsync_interval = vsync_interval_;
  last_presentation_time_ = feedback.timestamp;
  if (feedback.interval.is_positive()) {
    if (vsync_interval_ != feedback.interval) {
      DVLOG(1) << "OnPresentationFeedback: vsync interval updated to "
               << feedback.interval.InMillisecondsF() << "ms";
    }
    vsync_interval_ = feedback.interval;
  } else {
    // Reset to the initial safe default of 60 fps in case the
    // compositor started returning 0 after previously reporting a positive
    // value, e.g. the window moved to a different display or VRR was enabled.
    DVLOG(1) << "OnPresentationFeedback: feedback.interval=0, resetting to "
                "default "
             << kDefaultInterval.InMillisecondsF() << "ms";
    vsync_interval_ = kDefaultInterval;
  }

  // Invalidate the preferred interval from viz if the display's vsync changed.
  if (vsync_interval_ != previous_vsync_interval) {
    preferred_interval_ = base::TimeDelta();
  }
  if (delegate_ && vsync_interval_ != last_sent_vsync_interval_) {
    last_sent_vsync_interval_ = vsync_interval_;
    delegate_->OnVSyncIntervalChanged(last_presentation_time_, vsync_interval_);
  }
}

void BeginFrameSourceWayland::MaybeIssueBeginFrame() {
  TRACE_EVENT1("wayland", "BeginFrameSourceWayland::MaybeIssueBeginFrame",
               "effective_interval_us",
               GetEffectiveInterval().InMicroseconds());
  if (!needs_begin_frame_ || !ready_to_issue_begin_frame_ || frame_in_flight_ ||
      !delegate_) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();

  if (!last_frame_deadline_time_.is_null()) {
    // Wayland sometimes fires multiple frame callbacks within one refresh
    // cycle. Issuing faster than the display is wasteful and can cause
    // stalls since viz won't ack additional frames with the same frame_time,
    // which would happen below when we snap to vsync. So we defer until just
    // after the last frame's deadline to make sure the next frame will be
    // snapped to the next vsync.
    base::TimeDelta time_to_next_frame =
        last_frame_deadline_time_ - now + base::Microseconds(1);
    if (time_to_next_frame.is_positive()) {
      DVLOG(1) << "MaybeIssueBeginFrame: tried to issue too early, waiting "
               << time_to_next_frame.InMillisecondsF() << "ms";
      if (!deferred_issue_begin_frame_timer_.IsRunning()) {
        deferred_issue_begin_frame_timer_.Start(
            FROM_HERE, time_to_next_frame,
            base::BindOnce(&BeginFrameSourceWayland::MaybeIssueBeginFrame,
                           weak_factory_.GetWeakPtr()));
      }
      return;
    }
  }

  const base::TimeDelta effective_interval = GetEffectiveInterval();
  base::TimeTicks deadline = now + effective_interval;
  // The naive deadline of one interval from now is too far in the
  // future because "now" is some meaningful amount of time after the previous
  // frame was shown due to IPC delays, etc. If we have the last known
  // presentation time, we can calculate a deadline aligned to the
  // display's actual vsync/refresh cycle.
  if (!last_presentation_time_.is_null()) {
    deadline =
        now.SnappedToNextTick(last_presentation_time_, effective_interval);
  }
  // Likewise, we can determine the true frame time, which was immediately after
  // the previous frame was shown. This keeps the difference between frame time
  // and deadline equal to the interval. If we don't have presentation time,
  // frame_time will resolve to "now" and still be consistent with the deadline
  // of now + interval, but both will be out of sync with the display.
  base::TimeTicks frame_time = deadline - effective_interval;

  DVLOG(2) << "MaybeIssueBeginFrame:"
           << " vsync_interval=" << vsync_interval_.InMillisecondsF() << "ms"
           << " preferred_interval=" << preferred_interval_.InMillisecondsF()
           << "ms"
           << " effective_interval=" << effective_interval.InMillisecondsF()
           << "ms"
           << " frame_time began=" << (frame_time - now).InMillisecondsF()
           << "ms"
           << " deadline in=" << (deadline - now).InMillisecondsF() << "ms";

  ready_to_issue_begin_frame_ = false;
  frame_in_flight_ = true;

  last_frame_deadline_time_ = deadline;
  delegate_->OnBeginFrame(
      frame_time, deadline, effective_interval,
      base::BindOnce(&BeginFrameSourceWayland::OnBeginFrameAck,
                     weak_factory_.GetWeakPtr()));
}

void BeginFrameSourceWayland::OnBeginFrameAck(bool has_damage) {
  TRACE_EVENT1("wayland", "BeginFrameSourceWayland::OnBeginFrameAck",
               "has_damage", has_damage);
  frame_in_flight_ = false;
  if (!needs_begin_frame_) {
    return;
  }

  if (ready_to_issue_begin_frame_) {
    DVLOG(1)
        << "OnBeginFrameAck: next frame callback arrived early, attempting to "
           "issue immediately";
    MaybeIssueBeginFrame();
  } else if (!has_damage) {
    // No damage means no buffer commit, so no frame callback will arrive.
    // Request a bare frame callback from the compositor to maintain pacing.
    DVLOG(2) << "OnBeginFrameAck: no damage, requesting empty frame callback";
    frame_manager_->RequestFrameCallback();
    StartFrameCallbackTimer();
  } else {
    DVLOG(2) << "OnBeginFrameAck: has damage, waiting for frame callback";
    StartFrameCallbackTimer();
  }
}

void BeginFrameSourceWayland::StartFrameCallbackTimer() {
  frame_callback_timeout_timer_.Start(
      FROM_HERE, GetEffectiveInterval() * 2,
      base::BindOnce(&BeginFrameSourceWayland::OnFrameCallbackTimeout,
                     weak_factory_.GetWeakPtr()));
}

void BeginFrameSourceWayland::OnFrameCallbackTimeout() {
  TRACE_EVENT0("wayland", "BeginFrameSourceWayland::OnFrameCallbackTimeout");
  DVLOG(1) << "OnFrameCallbackTimeout: timed out with no frame callback, "
              "immediately issuing frame";
  if (!ready_to_issue_begin_frame_) {
    ready_to_issue_begin_frame_ = true;
    MaybeIssueBeginFrame();
  }
}

}  // namespace ui
