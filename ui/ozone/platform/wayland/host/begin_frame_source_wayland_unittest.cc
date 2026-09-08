// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/begin_frame_source_wayland.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/platform_window/extensions/begin_frame_source_extension.h"

namespace ui {

namespace {

// Common vsync intervals as compositors typically report them via
// wp_presentation feedback, which can differ slightly from the nominal values
// in viz.
constexpr base::TimeDelta k60Hz = base::Microseconds(16667);
constexpr base::TimeDelta k120Hz = base::Microseconds(8333);
constexpr base::TimeDelta k144Hz = base::Microseconds(6944);
constexpr base::TimeDelta k165Hz = base::Microseconds(6061);
constexpr base::TimeDelta k240Hz = base::Microseconds(4166);

base::TimeDelta Compute(base::TimeDelta preferred, base::TimeDelta vsync) {
  return BeginFrameSourceWayland::ComputeEffectiveInterval(preferred, vsync);
}

}  // namespace

// 60fps content on a 60hz display.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     VsyncPreferenceReturnsVsync) {
  EXPECT_EQ(k60Hz, Compute(k60Hz, k60Hz));
}
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     EvenlyDivisibleRatesAreSubsampled) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(33334), k60Hz));
  EXPECT_EQ(5 * k120Hz, Compute(base::Microseconds(41667), k120Hz));
  EXPECT_EQ(6 * k144Hz, Compute(base::Microseconds(41667), k144Hz));
  EXPECT_EQ(8 * k240Hz, Compute(base::Microseconds(33334), k240Hz));
  EXPECT_EQ(4 * k240Hz, Compute(base::Microseconds(16667), k240Hz));
}

// Close matches

// 23.976fps (NTSC) content on a 120Hz display is treated as 24fps.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NtscSnapsToNearestGrid) {
  EXPECT_EQ(5 * k120Hz, Compute(base::Microseconds(41708), k120Hz));
}
// Viz asks for a 16666us interval (60Hz) but the 60Hz display reports 16667us.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     AlignsWithMeasuredVsyncGrid60Hz) {
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(16666), k60Hz));
}
// Viz asks for a 33332us interval (30Hz) but the 60Hz display reports 16667us.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     AlignsWithMeasuredVsyncGrid30Hz) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(33332), k60Hz));
}

// A request within kDeltaAlmostEqual (10us) per vsync of a whole multiple
// still counts as that multiple; anything further under floors to the next
// faster grid rate.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, ToleranceBoundary) {
  EXPECT_EQ(2 * k60Hz, Compute(2 * k60Hz - base::Microseconds(20), k60Hz));
  EXPECT_EQ(k60Hz, Compute(2 * k60Hz - base::Microseconds(21), k60Hz));
}

TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     NonDivisibleRatesAreFlooredToNextFasterGrid) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(41667), k60Hz));
  EXPECT_EQ(2 * k144Hz, Compute(base::Microseconds(16667), k144Hz));
  EXPECT_EQ(2 * k165Hz, Compute(base::Microseconds(16667), k165Hz));
}

// Edge cases/caps

// No preference was set or it was invalidated.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NoPreferenceIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(base::TimeDelta(), k60Hz));
}
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NoVsyncReturnsVsync) {
  EXPECT_EQ(base::TimeDelta(), Compute(k60Hz, base::TimeDelta()));
}
// A preference that is too fast maxes at the display's full rate.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     FasterThanDisplayIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(k120Hz, k60Hz));
}
// Negative preferences are discarded.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     NegativePreferenceIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(-16667), k60Hz));
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(-4000), k60Hz));
}
// Slow preferences are capped at the nearest grid rate not slower than
// kMaxEffectiveInterval (24fps).
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, SlowPreferenceIsCapped) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Seconds(1), k60Hz));
  EXPECT_EQ(10 * k240Hz, Compute(base::Seconds(1), k240Hz));
}

//
// State machine tests
//

namespace {

class FakeBeginFrameDelegate : public BeginFrameSourceExtension::Delegate {
 public:
  void OnBeginFrame(base::TimeTicks frame_time,
                    base::TimeTicks deadline,
                    base::TimeDelta interval,
                    base::OnceCallback<void(bool has_damage)> ack) override {
    ++begin_frame_count;
    last_frame_time = frame_time;
    last_deadline = deadline;
    last_interval = interval;
    last_ack = std::move(ack);
    if (quit_closure && begin_frame_count >= quit_target) {
      std::move(quit_closure).Run();
    }
  }

  void OnVSyncIntervalChanged(base::TimeTicks timebase,
                              base::TimeDelta interval) override {
    ++vsync_changed_count;
    last_vsync_interval = interval;
  }

  // Acks the outstanding begin frame. There must be one in flight.
  void Ack(bool has_damage) {
    ASSERT_FALSE(last_ack.is_null());
    std::move(last_ack).Run(has_damage);
  }

  int begin_frame_count = 0;
  int vsync_changed_count = 0;
  base::TimeTicks last_frame_time;
  base::TimeTicks last_deadline;
  base::TimeDelta last_interval;
  base::TimeDelta last_vsync_interval;
  base::OnceCallback<void(bool)> last_ack;
  int quit_target = 0;
  base::OnceClosure quit_closure;
};

}  // namespace

class BeginFrameSourceWaylandTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    // Disable the feature so the window's WaylandFrameManager doesn't also
    // create a frame source, which could interfere.
    disabled_features_.push_back(features::kWaylandExternalBeginFrameSource);
    WaylandTestSimple::SetUp();

    // Wire the source into the frame manager under test so the frame
    // manager's notifications (Hide(), commits) reach it.
    frame_manager_ =
        std::make_unique<WaylandFrameManager>(window_.get(), connection_.get());
    frame_manager_->begin_frame_source_ =
        std::make_unique<BeginFrameSourceWayland>(window_.get(),
                                                  frame_manager_.get());
    source_ = frame_manager_->begin_frame_source_.get();
    source_->SetDelegate(&delegate_);
  }

 protected:
  static constexpr base::TimeDelta kDefaultInterval =
      BeginFrameSourceWayland::kDefaultInterval;

  // Injects a frame so we can test HasFrameWaitingToCommit().
  void AddPendingFrame() {
    frame_manager_->pending_frames_.push_back(std::make_unique<WaylandFrame>(
        window_->root_surface(), wl::WaylandOverlayConfig()));
  }

  bool RecoveryTimerRunning() const {
    return source_->frame_callback_recovery_timer_.IsRunning();
  }
  bool DeferredIssueTimerRunning() const {
    return source_->deferred_issue_begin_frame_timer_.IsRunning();
  }
  void FireRecoveryTimer() { source_->OnFrameCallbackRecoveryTimerFired(); }
  void set_last_frame_deadline(base::TimeTicks deadline) {
    source_->last_frame_deadline_time_ = deadline;
  }
  base::TimeTicks last_frame_deadline() const {
    return source_->last_frame_deadline_time_;
  }

  FakeBeginFrameDelegate delegate_;
  std::unique_ptr<WaylandFrameManager> frame_manager_;
  raw_ptr<BeginFrameSourceWayland> source_;
};

// SetNeedsBeginFrame(true) issues exactly one begin frame, one interval wide.
TEST_F(BeginFrameSourceWaylandTest, SetNeedsBeginFrameIssuesBeginFrame) {
  source_->SetNeedsBeginFrame(true);
  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_EQ(kDefaultInterval, delegate_.last_interval);
  EXPECT_EQ(kDefaultInterval,
            delegate_.last_deadline - delegate_.last_frame_time);
}

// No begin frames are produced until they are requested.
TEST_F(BeginFrameSourceWaylandTest, NoBeginFrameUntilNeeded) {
  EXPECT_EQ(0, delegate_.begin_frame_count);
  source_->OnFrameCallback(base::TimeTicks::Now());
  EXPECT_EQ(0, delegate_.begin_frame_count);
}

// An ack with damage waits for the frame callback and arms the recovery timer.
TEST_F(BeginFrameSourceWaylandTest, DamageAckWaitsForCallback) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);

  delegate_.Ack(/*has_damage=*/true);
  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_TRUE(RecoveryTimerRunning());
}

// A no-damage ack issues a begin frame for next tick.
TEST_F(BeginFrameSourceWaylandTest, NoDamageAckDrivesNextFrameFromGrid) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);

  delegate_.Ack(/*has_damage=*/false);
  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_TRUE(DeferredIssueTimerRunning());
  EXPECT_FALSE(RecoveryTimerRunning());
}

// A no-damage ack is ignored if a frame is still waiting to commit.
TEST_F(BeginFrameSourceWaylandTest, NoDamageAckWithPendingCommitWaits) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);

  AddPendingFrame();
  ASSERT_TRUE(frame_manager_->HasFrameWaitingToCommit());

  delegate_.Ack(/*has_damage=*/false);
  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_TRUE(RecoveryTimerRunning());
  EXPECT_FALSE(DeferredIssueTimerRunning());
}

// Hide() destroys the outstanding frame callback and drains pending frames.
TEST_F(BeginFrameSourceWaylandTest, HideUnblocksAwaitedCallback) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  AddPendingFrame();

  frame_manager_->Hide();

  EXPECT_TRUE(DeferredIssueTimerRunning());
  EXPECT_FALSE(frame_manager_->HasFrameWaitingToCommit());
}

// A frame callback that arrives before the ack marks the source ready, and
// when the ack arrives it issues a begin frame for the next vsync.
TEST_F(BeginFrameSourceWaylandTest, EarlyCallbackThenAckIssues) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);

  source_->OnFrameCallback(base::TimeTicks::Now());
  delegate_.Ack(/*has_damage=*/true);
  // Still within the current vsync, so the reissue is deferred.
  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_TRUE(DeferredIssueTimerRunning());
}

// Presentation feedback grants issue readiness after a damaging ack.
TEST_F(BeginFrameSourceWaylandTest, PresentationFeedbackGrantsReadiness) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  ASSERT_TRUE(RecoveryTimerRunning());
  ASSERT_FALSE(DeferredIssueTimerRunning());

  source_->OnPresentationFeedback(gfx::PresentationFeedback(
      base::TimeTicks::Now(), kDefaultInterval, /*flags=*/0));

  EXPECT_TRUE(DeferredIssueTimerRunning());
  EXPECT_FALSE(RecoveryTimerRunning());
}

// Suspension halts production and the stall timer. Unsuspending resumes.
TEST_F(BeginFrameSourceWaylandTest, SuspensionStopsAndResumes) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  ASSERT_TRUE(RecoveryTimerRunning());

  source_->OnWindowSuspensionChanged(true);
  EXPECT_FALSE(RecoveryTimerRunning());
  // A callback while suspended must not issue.
  source_->OnFrameCallback(base::TimeTicks::Now());
  EXPECT_EQ(1, delegate_.begin_frame_count);

  source_->OnWindowSuspensionChanged(false);
  EXPECT_TRUE(DeferredIssueTimerRunning());
}

// Presentation feedback updates the vsync interval and notifies the delegate,
// and subsequent begin frames use the new interval.
TEST_F(BeginFrameSourceWaylandTest, PresentationFeedbackUpdatesInterval) {
  source_->OnPresentationFeedback(gfx::PresentationFeedback(
      base::TimeTicks::Now(), base::Microseconds(8333), /*flags=*/0));
  EXPECT_EQ(1, delegate_.vsync_changed_count);
  EXPECT_EQ(base::Microseconds(8333), delegate_.last_vsync_interval);

  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  EXPECT_EQ(base::Microseconds(8333), delegate_.last_interval);
}

// A zero interval from feedback (e.g. VRR/display change) resets to the
// default.
TEST_F(BeginFrameSourceWaylandTest, PresentationFeedbackZeroResetsToDefault) {
  source_->OnPresentationFeedback(gfx::PresentationFeedback(
      base::TimeTicks::Now(), base::Microseconds(8333), /*flags=*/0));
  ASSERT_EQ(base::Microseconds(8333), delegate_.last_vsync_interval);

  source_->OnPresentationFeedback(gfx::PresentationFeedback(
      base::TimeTicks::Now(), base::TimeDelta(), /*flags=*/0));
  EXPECT_EQ(kDefaultInterval, delegate_.last_vsync_interval);
}

// A preferred interval slower than the display subsamples begin frames to a
// whole multiple of the vsync.
TEST_F(BeginFrameSourceWaylandTest, PreferredIntervalSubsamples) {
  source_->SetPreferredInterval(base::Microseconds(33333));
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  EXPECT_EQ(2 * kDefaultInterval, delegate_.last_interval);
}

// Reset() clears deadline state, so a fresh cycle issues immediately rather
// than inheriting the previous deadline and deferring against it.
TEST_F(BeginFrameSourceWaylandTest, ResetClearsDeadlineState) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);  // last_frame_deadline_time_ is now set

  source_->Reset();

  // A fresh start issues right away; without the deadline clear it would hit
  // the throttle against the old deadline and defer instead.
  source_->SetNeedsBeginFrame(true);
  EXPECT_EQ(2, delegate_.begin_frame_count);
}

// End to end: after a damaging ack and its frame callback, the deferred timer
// fires and the next begin frame is issued.
TEST_F(BeginFrameSourceWaylandTest, DeferredFrameEventuallyIssues) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  source_->OnFrameCallback(base::TimeTicks::Now());
  ASSERT_TRUE(DeferredIssueTimerRunning());

  base::RunLoop run_loop;
  delegate_.quit_target = 2;
  delegate_.quit_closure = run_loop.QuitClosure();
  run_loop.Run();
  EXPECT_EQ(2, delegate_.begin_frame_count);
}

// If the frame callback never arrives, the recovery timer breaks the stall by
// issuing a frame.
TEST_F(BeginFrameSourceWaylandTest, RecoveryTimerReissues) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  ASSERT_TRUE(RecoveryTimerRunning());

  FireRecoveryTimer();
  EXPECT_TRUE(DeferredIssueTimerRunning());
}

// With presentation feedback, the begin frame deadline is snapped to the
// compositor's vsync grid instead of a naive now + interval.
TEST_F(BeginFrameSourceWaylandTest, DeadlineSnapsToVsyncGrid) {
  // Exactly on a tick: the strictly-forward snap grants the full interval
  // rather than a zero-width deadline.
  const base::TimeTicks timebase = base::TimeTicks::Now();
  const base::TimeDelta interval = base::Microseconds(8333);
  source_->OnPresentationFeedback(
      gfx::PresentationFeedback(timebase, interval, /*flags=*/0));

  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  EXPECT_EQ(interval, delegate_.last_interval);
  EXPECT_EQ(timebase + interval, delegate_.last_deadline);

  // Mid-cycle: the deadline targets the next tick, issued immediately.
  source_->Reset();
  const base::TimeTicks mid_cycle_timebase =
      base::TimeTicks::Now() - base::Microseconds(4000);
  source_->OnPresentationFeedback(gfx::PresentationFeedback(
      mid_cycle_timebase, kDefaultInterval, /*flags=*/0));
  source_->SetNeedsBeginFrame(true);

  ASSERT_EQ(2, delegate_.begin_frame_count);
  EXPECT_FALSE(DeferredIssueTimerRunning());
  EXPECT_EQ(mid_cycle_timebase + kDefaultInterval, delegate_.last_deadline);
}

// After a long pause/suspension/stall the last deadline is stale. The resumed
// frame issues immediately against the vsync grid.
TEST_F(BeginFrameSourceWaylandTest, StaleDeadlineResumesImmediately) {
  const base::TimeTicks timebase = base::TimeTicks::Now();
  source_->OnPresentationFeedback(
      gfx::PresentationFeedback(timebase, kDefaultInterval, /*flags=*/0));

  // Simulate a long pause: the last issued deadline is a second in the past.
  set_last_frame_deadline(timebase - base::Seconds(1));
  source_->SetNeedsBeginFrame(true);

  EXPECT_EQ(1, delegate_.begin_frame_count);
  EXPECT_FALSE(DeferredIssueTimerRunning());

  EXPECT_GT(last_frame_deadline(), timebase);
  const int64_t phase_us = (last_frame_deadline() - timebase).InMicroseconds() %
                           kDefaultInterval.InMicroseconds();
  EXPECT_EQ(0, phase_us);
}

// Clearing the need for begin frames stops production and cancels timers.
TEST_F(BeginFrameSourceWaylandTest, ClearingNeedStopsProduction) {
  source_->SetNeedsBeginFrame(true);
  ASSERT_EQ(1, delegate_.begin_frame_count);
  delegate_.Ack(/*has_damage=*/true);
  ASSERT_TRUE(RecoveryTimerRunning());

  source_->SetNeedsBeginFrame(false);
  EXPECT_FALSE(RecoveryTimerRunning());
  EXPECT_FALSE(DeferredIssueTimerRunning());
  source_->OnFrameCallback(base::TimeTicks::Now());
  EXPECT_EQ(1, delegate_.begin_frame_count);
}

}  // namespace ui
