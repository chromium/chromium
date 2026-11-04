// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/begin_frame_source_wayland.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"

namespace ui {

BeginFrameSourceWayland::BeginFrameSourceWayland(
    PlatformWindow* window,
    WaylandFrameManager* frame_manager)
    : frame_manager_(frame_manager), window_(window) {
  CHECK(frame_manager_, base::NotFatalUntil::M163);
  SetBeginFrameSourceExtension(window, this);
}

BeginFrameSourceWayland::~BeginFrameSourceWayland() {
  SetBeginFrameSourceExtension(window_, nullptr);
}

void BeginFrameSourceWayland::Reset() {
  needs_begin_frame_ = false;
  frame_in_flight_ = false;
  ready_to_issue_begin_frame_ = false;
  last_frame_deadline_time_ = base::TimeTicks();
  last_sent_vsync_interval_ = base::TimeDelta();
  UpdateBeginFrameProduction();
}

void BeginFrameSourceWayland::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void BeginFrameSourceWayland::SetNeedsBeginFrame(bool needs) {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::SetNeedsBeginFrame",
              "needs_begin_frames", needs, "surface_id",
              frame_manager_->GetRootSurfaceId());
  if (needs_begin_frame_ == needs) {
    return;
  }
  needs_begin_frame_ = needs;
  UpdateBeginFrameProduction();
}

void BeginFrameSourceWayland::SetPreferredInterval(base::TimeDelta interval) {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::SetPreferredInterval",
              "interval_us", interval.InMicroseconds(), "surface_id",
              frame_manager_->GetRootSurfaceId());
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
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::OnFrameCallback",
              "callback_time", callback_time, "surface_id",
              frame_manager_->GetRootSurfaceId());
  ready_to_issue_begin_frame_ = true;
  UpdateFrameCallbackRecoveryTimer();

  if (!frame_in_flight_) {
    MaybeIssueBeginFrame();
  }
}

void BeginFrameSourceWayland::OnFrameCallbackUnavailable() {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::OnFrameCallbackUnavailable",
              "surface_id", frame_manager_->GetRootSurfaceId());
  ready_to_issue_begin_frame_ = true;
  UpdateFrameCallbackRecoveryTimer();
  MaybeIssueBeginFrame();
}

void BeginFrameSourceWayland::OnPresentationFeedback(
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::OnPresentationFeedback",
              "interval_us", feedback.interval.InMicroseconds(), "failed",
              feedback.failed(), "surface_id",
              frame_manager_->GetRootSurfaceId());
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

  // Presentation is authoritative proof that the compositor is ready for a
  // frame.
  ready_to_issue_begin_frame_ = true;
  UpdateFrameCallbackRecoveryTimer();
  if (!frame_in_flight_) {
    MaybeIssueBeginFrame();
  }
}

void BeginFrameSourceWayland::MaybeIssueBeginFrame() {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::MaybeIssueBeginFrame",
              "effective_interval_us", GetEffectiveInterval().InMicroseconds(),
              "surface_id", frame_manager_->GetRootSurfaceId());
  if (!needs_begin_frame_ || suspended_ || !ready_to_issue_begin_frame_ ||
      frame_in_flight_ || !delegate_) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta effective_interval = GetEffectiveInterval();

  // Issue at most one begin frame per vsync cycle, because:
  //
  // 1) viz won't ack additional frames with the same frame time.
  // 2) it's wasteful to produce frames faster than the display.
  //
  if (!last_frame_deadline_time_.is_null()) {
    base::TimeDelta time_to_next_frame =
        last_frame_deadline_time_ - now + base::Microseconds(1);
    if (time_to_next_frame.is_positive()) {
      DeferIssueToNextFrame("attempted to issue before deadline",
                            time_to_next_frame);
      return;
    }
  }

  // Derive the true deadline (vsync) from the last presentation time,
  // falling back to the approximation of one interval from now.
  base::TimeTicks deadline = now + effective_interval;
  if (!last_presentation_time_.is_null()) {
    // Add a tiny value so we snap to the *next* vsync in case now == vsync.
    deadline =
        (now + base::Microseconds(1))
            .SnappedToNextTick(last_presentation_time_, effective_interval);

    // Apply a half-interval margin to further prevent multiple frames from
    // issuing during the same vsync cycle. This catches compositors that
    // sometimes send late presentation timestamps (Mutter), causing us to
    // compute the wrong next deadline. Similar to kDoubleTickDivisor in viz.
    if (!last_frame_deadline_time_.is_null() &&
        deadline - last_frame_deadline_time_ < effective_interval / 2) {
      DeferIssueToNextFrame("attempted to issue within half interval margin",
                            deadline - now + base::Microseconds(1));
      return;
    }
  }

  // The frame time should line up with the previous vsync. But if the
  // interval changes, that could put it before the previous deadline, causing
  // viz to drop the begin frame without acking. So we clamp as in
  // DelayBasedBeginFrameSource::OnTimerTick.
  base::TimeTicks frame_time =
      std::max(deadline - effective_interval, last_frame_deadline_time_);

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
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::OnBeginFrameAck",
              "has_damage", has_damage, "surface_id",
              frame_manager_->GetRootSurfaceId());
  frame_in_flight_ = false;
  if (!needs_begin_frame_) {
    UpdateFrameCallbackRecoveryTimer();
    return;
  }

  if (ready_to_issue_begin_frame_) {
    TRACE_EVENT_INSTANT("wayland",
                        "OnBeginFrameAck: callback arrived early, issuing");
    MaybeIssueBeginFrame();
  } else if (has_damage) {
    TRACE_EVENT_INSTANT("wayland",
                        "OnBeginFrameAck: damage, awaiting callback");
  } else if (frame_manager_->HasFrameWaitingToCommit()) {
    TRACE_EVENT_INSTANT(
        "wayland", "OnBeginFrameAck: frame waiting to commit, not issuing");
  } else {
    // No damage means no buffer commit, so no frame callback or presentation
    // feedback will arrive. Issue a frame for the next vsync to maintain
    // pacing.
    TRACE_EVENT_INSTANT("wayland",
                        "OnBeginFrameAck: no damage, scheduling next frame");
    ready_to_issue_begin_frame_ = true;
    MaybeIssueBeginFrame();
  }
  UpdateFrameCallbackRecoveryTimer();
}

void BeginFrameSourceWayland::UpdateFrameCallbackRecoveryTimer() {
  // TODO(crbug.com/537421794): Callbacks arriving late (even after multiple
  // vsyncs) is fine and expected; it is only a problem if they never arrive at
  // all but we were waiting for them. This fallback mechanism should be relaxed
  // as we address internal causes for lost frame callbacks and improve pacing
  // and recovery techniques.
  static constexpr base::TimeDelta kFrameCallbackRecoveryTimeout =
      base::Milliseconds(100);
  const bool waiting_for_frame_callback =
      needs_begin_frame_ && !frame_in_flight_ && !ready_to_issue_begin_frame_;
  if (waiting_for_frame_callback && !suspended_) {
    frame_callback_recovery_timer_.Start(
        FROM_HERE, kFrameCallbackRecoveryTimeout,
        base::BindOnce(
            &BeginFrameSourceWayland::OnFrameCallbackRecoveryTimerFired,
            weak_factory_.GetWeakPtr()));
  } else {
    frame_callback_recovery_timer_.Stop();
  }
}

void BeginFrameSourceWayland::OnWindowSuspensionChanged(bool suspended) {
  TRACE_EVENT("wayland", "BeginFrameSourceWayland::OnWindowSuspensionChanged",
              "suspended", suspended, "surface_id",
              frame_manager_->GetRootSurfaceId());
  if (suspended_ == suspended) {
    return;
  }
  suspended_ = suspended;
  UpdateBeginFrameProduction();
}

void BeginFrameSourceWayland::UpdateBeginFrameProduction() {
  if (needs_begin_frame_ && !suspended_) {
    if (!frame_in_flight_ && !frame_manager_->HasFrameWaitingToCommit()) {
      // Force the first frame after starting/resuming production since
      // we won't have any readiness signals yet.
      ready_to_issue_begin_frame_ = true;
      MaybeIssueBeginFrame();
    }
  } else {
    deferred_issue_begin_frame_timer_.Stop();
  }
  UpdateFrameCallbackRecoveryTimer();
}

void BeginFrameSourceWayland::DeferIssueToNextFrame(const char* reason,
                                                    base::TimeDelta delay) {
  if (deferred_issue_begin_frame_timer_.IsRunning()) {
    return;
  }
  TRACE_EVENT_INSTANT("wayland", "MaybeIssueBeginFrame: deferring", "reason",
                      reason, "wait_us", delay.InMicroseconds());
  deferred_issue_begin_frame_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&BeginFrameSourceWayland::MaybeIssueBeginFrame,
                     weak_factory_.GetWeakPtr()));
}

void BeginFrameSourceWayland::OnFrameCallbackRecoveryTimerFired() {
  TRACE_EVENT("wayland",
              "BeginFrameSourceWayland::OnFrameCallbackRecoveryTimerFired",
              "surface_id", frame_manager_->GetRootSurfaceId());
  if (!ready_to_issue_begin_frame_) {
    DVLOG(1) << "OnFrameCallbackRecoveryTimerFired: no frame callback arrived, "
                "issuing a "
                "frame to recover";
    ready_to_issue_begin_frame_ = true;
    MaybeIssueBeginFrame();
  }
}

}  // namespace ui
