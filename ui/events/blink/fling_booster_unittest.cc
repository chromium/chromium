// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/fling_booster.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_modifiers.h"

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using gfx::Vector2dF;

namespace ui {
namespace test {

static constexpr base::TimeDelta kEventDelta = base::Milliseconds(10);

// Constants from fling_booster.cc
static constexpr double kMinBoostScrollSpeed = 150.;
static constexpr double kMinBoostFlingSpeed = 350.;
static constexpr base::TimeDelta kFlingBoostTimeoutDelay = base::Seconds(0.05);

class FlingBoosterTest : public testing::Test {
 public:
  FlingBoosterTest() = default;

  WebGestureEvent CreateFlingStart(
      const gfx::Vector2dF& velocity,
      int modifiers = 0,
      WebGestureDevice source_device = WebGestureDevice::kTouchscreen) {
    WebGestureEvent fling_start(WebInputEvent::Type::kGestureFlingStart,
                                modifiers, event_time_, source_device);
    fling_start.data.fling_start.velocity_x = velocity.x();
    fling_start.data.fling_start.velocity_y = velocity.y();
    return fling_start;
  }

  WebGestureEvent CreateFlingCancel(
      WebGestureDevice source_device = WebGestureDevice::kTouchscreen) {
    WebGestureEvent fling_cancel(WebInputEvent::Type::kGestureFlingCancel, 0,
                                 event_time_, source_device);
    return fling_cancel;
  }

  WebGestureEvent CreateScrollBegin(
      gfx::Vector2dF delta,
      WebGestureDevice source_device = WebGestureDevice::kTouchscreen) {
    WebGestureEvent scroll_begin(WebInputEvent::Type::kGestureScrollBegin, 0,
                                 event_time_, source_device);
    scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
    scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
    scroll_begin.data.scroll_begin.delta_hint_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    return scroll_begin;
  }

  WebGestureEvent CreateScrollUpdate(
      gfx::Vector2dF delta,
      WebGestureDevice source_device = WebGestureDevice::kTouchscreen) {
    WebGestureEvent scroll_update(WebInputEvent::Type::kGestureScrollUpdate, 0,
                                  event_time_, source_device);
    scroll_update.data.scroll_update.delta_x = delta.x();
    scroll_update.data.scroll_update.delta_y = delta.y();
    scroll_update.data.scroll_update.delta_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    return scroll_update;
  }

  WebGestureEvent CreateScrollEnd(
      WebGestureDevice source_device = WebGestureDevice::kTouchscreen) {
    return WebGestureEvent(WebInputEvent::Type::kGestureScrollEnd, 0,
                           event_time_, source_device);
  }

  Vector2dF DeltaFromVelocity(Vector2dF velocity, base::TimeDelta delta) {
    float delta_seconds = static_cast<float>(delta.InSecondsF());
    Vector2dF out = velocity;
    out.InvScale(delta_seconds);
    return out;
  }

  Vector2dF SendFlingStart(WebGestureEvent event) {
    DCHECK_EQ(WebInputEvent::Type::kGestureFlingStart, event.GetType());

    // The event will first be observed, then the FlingController will request
    // a possibly boosted velocity.
    fling_booster_.ObserveGestureEvent(event);
    return fling_booster_.GetVelocityForFlingStart(event);
  }

  // Simulates the gesture scroll stream for a scroll that should create a
  // boost.
  void SimulateBoostingScroll() {
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateFlingCancel());
    fling_booster_.ObserveGestureEvent(CreateScrollEnd());
    fling_booster_.ObserveGestureEvent(CreateScrollBegin(Vector2dF(0, 1)));

    // GestureScrollUpdates in the same direction and at sufficient speed should
    // be considered boosting. First GSU speed is ignored since we need 2 to
    // determine velocity.
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(0, 1)));
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateScrollUpdate(
        DeltaFromVelocity(Vector2dF(0, kMinBoostScrollSpeed), kEventDelta)));
  }

  void ProgressFling(const Vector2dF& current_velocity,
                     const Vector2dF& delta) {
    fling_booster_.ObserveGestureEvent(CreateScrollUpdate(delta));
    fling_booster_.ObserveProgressFling(current_velocity);
  }

 protected:
  base::TimeTicks event_time_ = base::TimeTicks() + base::Seconds(100000);
  FlingBooster fling_booster_;
};

TEST_F(FlingBoosterTest, FlingBoostBasic) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  EXPECT_EQ(Vector2dF(0, 1000), fling_velocity)
      << "First fling shouldn't be boosted";

  SimulateBoostingScroll();

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 3000), fling_velocity)
      << "FlingStart with ongoing fling should be boosted";
}

TEST_F(FlingBoosterTest, FlingBoostProgressedFling) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));

  event_time_ += kEventDelta;
  ProgressFling(Vector2dF(0, kMinBoostFlingSpeed), Vector2dF(0, 20));

  SimulateBoostingScroll();

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1100)));

  EXPECT_EQ(Vector2dF(0, 2100), fling_velocity) << "Fling should be boosted";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfScrollDelayed) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  // Delay longer than the timeout and ensure we don't boost.
  event_time_ += kFlingBoostTimeoutDelay + base::Milliseconds(1);
  fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(0, 10000)));

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "ScrollUpdate delayed longer than boosting timeout; fling shouldn't "
         "be boosted.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfBoostTooSlow) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  auto new_velocity = Vector2dF(0, kMinBoostFlingSpeed - 1);
  fling_velocity = SendFlingStart(CreateFlingStart(new_velocity));
  EXPECT_EQ(new_velocity, fling_velocity)
      << "Boosting FlingStart too slow; fling shouldn't be boosted.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfPreviousStartVelocityTooSlow) {
  Vector2dF fling_velocity;

  fling_velocity =
      SendFlingStart(CreateFlingStart(Vector2dF(0, kMinBoostFlingSpeed - 1)));

  SimulateBoostingScroll();
  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "Previous fling too slow and shouldn't be boosted.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfCurrentVelocityTooSlow) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));

  event_time_ += kEventDelta;
  ProgressFling(Vector2dF(0, kMinBoostFlingSpeed - 1), Vector2dF(0, 20));

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1100)));

  EXPECT_EQ(Vector2dF(0, 1100), fling_velocity)
      << "Existing fling too slow and shouldn't be boosted.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfFlingInDifferentDirection) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(1000, 0)));
  EXPECT_EQ(Vector2dF(1000, 0), fling_velocity)
      << "Fling isn't in same direction, shouldn't boost.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfScrollInDifferentDirection) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  // Start a new scroll in an orthogonal direction and fling in the direction
  // of the original fling.
  event_time_ += kEventDelta;
  fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(1000, 0)));

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "Scrolling in an orthogonal direction should prevent boosting, even "
         "if the fling is in the original direction.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfPreventBoostingFlagIsSet) {
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(0, 1000));

  Vector2dF fling_velocity = SendFlingStart(fling_start);

  // Start a new scroll.
  event_time_ += kEventDelta;
  WebGestureEvent cancel_event = CreateFlingCancel();
  cancel_event.data.fling_cancel.prevent_boosting = true;
  fling_booster_.ObserveGestureEvent(cancel_event);
  fling_booster_.ObserveGestureEvent(CreateScrollBegin(Vector2dF(0, 1)));

  // GestureScrollUpdates in the same direction and at sufficient speed should
  // be considered boosting. However, since the prevent_boosting flag was set,
  // we shouldn't boost.
  event_time_ += kEventDelta;
  fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(0, 10000)));
  event_time_ += kEventDelta;
  fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(0, 10000)));

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "prevent_boosting on FlingCancel should avoid boosting a subsequent "
         "FlingStart";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfDifferentFlingModifiers) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  fling_velocity =
      SendFlingStart(CreateFlingStart(Vector2dF(0, 2000), MODIFIER_SHIFT));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "Changed modifier keys should prevent boost.";
}

TEST_F(FlingBoosterTest, NoFlingBoostIfDifferentFlingSourceDevices) {
  Vector2dF fling_velocity;

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 1000)));
  SimulateBoostingScroll();

  fling_velocity = SendFlingStart(
      CreateFlingStart(Vector2dF(0, 1000), 0, WebGestureDevice::kTouchpad));
  EXPECT_EQ(Vector2dF(0, 1000), fling_velocity)
      << "Changed modifier keys should prevent boost.";
}

// Ensure a scroll starting after the boosting cutoff time doesn't extend the
// timeout.
TEST_F(FlingBoosterTest, NoFlingBoostIfScrollBeginPastCutoffTime) {
  WebGestureEvent fling_start = CreateFlingStart(Vector2dF(0, 1000));
  Vector2dF fling_velocity = SendFlingStart(fling_start);
  ASSERT_EQ(Vector2dF(0, 1000), fling_velocity);

  // Simulate a new fling boost delayed by more than the timeout.
  {
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateFlingCancel());
    fling_booster_.ObserveGestureEvent(CreateScrollEnd());
    event_time_ += kFlingBoostTimeoutDelay + base::Milliseconds(1);
    fling_booster_.ObserveGestureEvent(CreateScrollBegin(Vector2dF(0, 1)));

    // GestureScrollUpdates in the same direction and at sufficient speed should
    // be considered boosting. First GSU speed is ignored since we need 2 to
    // determine velocity.
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateScrollUpdate(Vector2dF(0, 1)));
    event_time_ += kEventDelta;
    fling_booster_.ObserveGestureEvent(CreateScrollUpdate(
        DeltaFromVelocity(Vector2dF(0, kMinBoostScrollSpeed), kEventDelta)));
  }

  fling_velocity = SendFlingStart(CreateFlingStart(Vector2dF(0, 2000)));
  EXPECT_EQ(Vector2dF(0, 2000), fling_velocity)
      << "Scroll must not be boosted since it occured after the boost timeout "
         "delay.";
}

}  // namespace test
}  // namespace ui
