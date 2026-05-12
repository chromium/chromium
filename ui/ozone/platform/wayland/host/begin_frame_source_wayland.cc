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
  }
}

void BeginFrameSourceWayland::SetPreferredInterval(base::TimeDelta interval) {
  if (!interval.is_zero()) {
    vsync_interval_ = interval;
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
  base::TimeTicks deadline = now + vsync_interval_;
  // The naive deadline of one vsync/refresh from now is too far in the
  // future because "now" is some meaningful amount of time after the previous
  // frame was shown due to IPC delays, etc. If we have the last frame's
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
    DVLOG(1) << "OnBeginFrameAck: frame callback arrived early, immediately "
                "issuing frame";
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
  DVLOG(2) << "StartFrameCallbackTimer: timeout="
           << (vsync_interval_ * 2).InMillisecondsF() << "ms";
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
