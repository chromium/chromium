// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/begin_frame_source_wayland.h"

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
  if (!interval.is_zero()) {
    DVLOG(1) << "SetPreferredInterval: preferred interval updated to "
             << interval.InMillisecondsF() << "ms";
    // TODO(crbug.com/513613495): Unused until it is set correctly.
    preferred_interval_ = interval;
  }
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
  if (!feedback.failed()) {
    last_presentation_time_ = feedback.timestamp;
    if (!feedback.interval.is_zero()) {
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
  }
}

void BeginFrameSourceWayland::MaybeIssueBeginFrame() {
  TRACE_EVENT0("wayland", "BeginFrameSourceWayland::MaybeIssueBeginFrame");
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

  base::TimeTicks deadline = now + vsync_interval_;
  // The naive deadline of one vsync/refresh from now is too far in the
  // future because "now" is some meaningful amount of time after the previous
  // frame was shown due to IPC delays, etc. If we have the last known
  // presentation time, we can calculate a deadline aligned to the
  // display's actual vsync/refresh cycle.
  if (!last_presentation_time_.is_null()) {
    deadline = now.SnappedToNextTick(last_presentation_time_, vsync_interval_);
  }
  // Likewise, we can determine the true frame time, which was immediately after
  // the previous frame was shown. This keeps the difference between frame time
  // and deadline equal to the vsync interval. If we don't have presentation
  // time, frame_time will resolve to "now" and still be consistent with the
  // deadline of now + vsync, but both will be out of sync with the display.
  base::TimeTicks frame_time = deadline - vsync_interval_;

  DVLOG(2) << "MaybeIssueBeginFrame:"
           << " vsync_interval=" << vsync_interval_.InMillisecondsF() << "ms"
           << " frame_time began=" << (frame_time - now).InMillisecondsF()
           << "ms"
           << " deadline in=" << (deadline - now).InMillisecondsF() << "ms";

  ready_to_issue_begin_frame_ = false;
  frame_in_flight_ = true;

  // TODO(crbug.com/513613495): Once preferred_interval_ is set correctly, use
  // a subsampling factor to scale the deadline and interval.
  last_frame_deadline_time_ = deadline;
  delegate_->OnBeginFrame(
      frame_time, deadline, vsync_interval_,
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
      FROM_HERE, vsync_interval_ * 2,
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
