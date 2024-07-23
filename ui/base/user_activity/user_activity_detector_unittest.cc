// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/user_activity/user_activity_detector.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// Implementation that just counts the number of times we've been told that the
// user is active.
class TestUserActivityObserver : public UserActivityObserver {
 public:
  TestUserActivityObserver() : num_invocations_(0) {}

  TestUserActivityObserver(const TestUserActivityObserver&) = delete;
  TestUserActivityObserver& operator=(const TestUserActivityObserver&) = delete;

  int num_invocations() const { return num_invocations_; }
  void reset_stats() { num_invocations_ = 0; }

  // UserActivityObserver implementation.
  void OnUserActivity(const ui::Event* event) override { num_invocations_++; }

 private:
  // Number of times that OnUserActivity() has been called.
  int num_invocations_;
};

// A test implementation of PlatformEventSource that we can instantiate to make
// sure that the PlatformEventSource has an instance while in unit tests.
class TestPlatformEventSource : public PlatformEventSource {
 public:
  TestPlatformEventSource() {}

  TestPlatformEventSource(const TestPlatformEventSource&) = delete;
  TestPlatformEventSource& operator=(const TestPlatformEventSource&) = delete;

  ~TestPlatformEventSource() override {}
};

class UserActivityDetectorTest : public testing::Test {
 public:
  UserActivityDetectorTest()
      : platform_event_source_(std::make_unique<TestPlatformEventSource>()),
        detector_(ui::UserActivityDetector::Get()),
        observer_(std::make_unique<TestUserActivityObserver>()) {
    platform_event_source_->RemovePlatformEventObserver(detector_.get());
    detector_->InitPlatformEventSourceObservationForTesting();
    detector_->AddObserver(observer_.get());
    now_ = base::TimeTicks::Now();
    detector_->set_now_for_test(now_);
  }

  UserActivityDetectorTest(const UserActivityDetectorTest&) = delete;
  UserActivityDetectorTest& operator=(const UserActivityDetectorTest&) = delete;

  ~UserActivityDetectorTest() override {
    detector_->RemoveObserver(observer_.get());
    detector_->ResetStateForTesting();
  }

 protected:
  // Move |detector_|'s idea of the current time forward by |delta|.
  void AdvanceTime(base::TimeDelta delta) {
    now_ += delta;
    detector_->set_now_for_test(now_);
  }

  void OnEvent(const ui::Event* event) {
    detector_->ProcessReceivedEvent(event);
  }

  std::unique_ptr<TestPlatformEventSource> platform_event_source_;
  raw_ptr<UserActivityDetector> detector_;
  std::unique_ptr<TestUserActivityObserver> observer_;

  base::TimeTicks now_;
};

// Checks that the observer is notified in response to different types of input
// events.
TEST_F(UserActivityDetectorTest, Basic) {
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  OnEvent(&key_event);
  EXPECT_FALSE(key_event.handled());
  EXPECT_EQ(now_, detector_->last_activity_time());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeDelta advance_delta =
      base::Milliseconds(UserActivityDetector::kNotifyIntervalMs);
  AdvanceTime(advance_delta);
  ui::MouseEvent mouse_event(ui::EventType::kMouseMoved, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE,
                             ui::EF_NONE);
  OnEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(now_, detector_->last_activity_time());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeTicks time_before_ignore = now_;

  // Temporarily ignore mouse events when displays are turned on or off.
  detector_->OnDisplayPowerChanging();
  OnEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(time_before_ignore, detector_->last_activity_time());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  const base::TimeDelta kIgnoreMouseTime = base::Milliseconds(
      UserActivityDetector::kDisplayPowerChangeIgnoreMouseMs);
  AdvanceTime(kIgnoreMouseTime / 2);
  OnEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(time_before_ignore, detector_->last_activity_time());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // After enough time has passed, mouse events should be reported again.
  AdvanceTime(std::max(kIgnoreMouseTime, advance_delta));
  OnEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(now_, detector_->last_activity_time());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  AdvanceTime(advance_delta);
  ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, gfx::Point(), base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  OnEvent(&touch_event);
  EXPECT_FALSE(touch_event.handled());
  EXPECT_EQ(now_, detector_->last_activity_time());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  AdvanceTime(advance_delta);
  ui::GestureEvent gesture_event(
      0, 0, ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  OnEvent(&gesture_event);
  EXPECT_FALSE(gesture_event.handled());
  EXPECT_EQ(now_, detector_->last_activity_time());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();
}

// Checks that observers aren't notified too frequently.
TEST_F(UserActivityDetectorTest, RateLimitNotifications) {
  // The observer should be notified about a key event.
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  OnEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  // It shouldn't be notified if a second event occurs in the same instant in
  // time.
  OnEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // Advance the time, but not quite enough for another notification to be sent.
  AdvanceTime(
      base::Milliseconds(UserActivityDetector::kNotifyIntervalMs - 100));
  OnEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // Advance time by the notification interval, definitely moving out of the
  // rate limit. This should let us trigger another notification.
  AdvanceTime(base::Milliseconds(UserActivityDetector::kNotifyIntervalMs));

  OnEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(1, observer_->num_invocations());
}

// Checks that the detector ignores synthetic mouse events.
TEST_F(UserActivityDetectorTest, IgnoreSyntheticMouseEvents) {
  ui::MouseEvent mouse_event(ui::EventType::kMouseMoved, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, ui::EF_NONE);
  OnEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(base::TimeTicks(), detector_->last_activity_time());
  EXPECT_EQ(0, observer_->num_invocations());
}

// Checks that observers are notified about externally-reported user activity.
TEST_F(UserActivityDetectorTest, HandleExternalUserActivity) {
  detector_->HandleExternalUserActivity();
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeDelta advance_delta =
      base::Milliseconds(UserActivityDetector::kNotifyIntervalMs);
  AdvanceTime(advance_delta);
  detector_->HandleExternalUserActivity();
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeDelta half_advance_delta =
      base::Milliseconds(UserActivityDetector::kNotifyIntervalMs / 2);
  AdvanceTime(half_advance_delta);
  detector_->HandleExternalUserActivity();
  EXPECT_EQ(0, observer_->num_invocations());

  AdvanceTime(half_advance_delta);
  detector_->HandleExternalUserActivity();
  EXPECT_EQ(1, observer_->num_invocations());
}

}  // namespace ui
