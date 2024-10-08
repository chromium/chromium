// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/input.h>
#include <stddef.h>

#include <cmath>
#include <limits>

#include "base/android/jni_android.h"
#include "base/numerics/angle_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"

namespace ui {

class MotionEvent;

namespace {

constexpr float kPixToDip = 0.5f;

constexpr int kAndroidActionButton = 0;
constexpr int kAndroidActionDown = AMOTION_EVENT_ACTION_DOWN;
constexpr int kAndroidActionPointerDown = AMOTION_EVENT_ACTION_POINTER_DOWN;
constexpr int kAndroidAltKeyDown = AMETA_ALT_ON;

// Corresponds to TOOL_TYPE_FINGER, see
// developer.android.com/reference/android/view/MotionEvent.html
//     #TOOL_TYPE_FINGER.
constexpr int kAndroidToolTypeFinger = 1;

// Corresponds to BUTTON_PRIMARY, see
// developer.android.com/reference/android/view/MotionEvent.html#BUTTON_PRIMARY.
constexpr int kAndroidButtonPrimary = 1;

// This function convert tilt_x and tilt_y back to tilt_rad.
float ConvertToTiltRad(float tilt_x, float tilt_y) {
  float tilt_x_r = sinf(base::DegToRad(tilt_x));
  float tilt_x_z = cosf(base::DegToRad(tilt_x));
  float tilt_y_r = sinf(base::DegToRad(tilt_y));
  float tilt_y_z = cosf(base::DegToRad(tilt_y));
  float r_x = tilt_x_r * tilt_y_z;
  float r_y = tilt_y_r * tilt_x_z;
  float r = sqrtf(r_x * r_x + r_y * r_y);
  float z = tilt_x_z * tilt_y_z;
  return atan2f(r, z);
}

}  // namespace

constexpr float float_error = 0.0001f;
// Note that these tests avoid creating a Java instance of the MotionEvent, as
// we're primarily testing caching behavior, and the code necessary to
// construct a Java-backed MotionEvent itself adds unnecessary complexity.
TEST(MotionEventAndroidTest, Constructor) {
  constexpr int kLatestEventTimeNS = 5'123'456;
  constexpr int kOldestEventTimeNS = 4'123'456;
  base::TimeTicks latest_event_time =
      base::TimeTicks() + base::Nanoseconds(kLatestEventTimeNS);
  base::TimeTicks oldest_event_time =
      base::TimeTicks() + base::Nanoseconds(kOldestEventTimeNS);

  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(latest_event_time);

  MotionEventAndroid::Pointer p0(
      1, 13.7f, -7.13f, 5.3f, 1.2f, 0.1f, 0.2f, kAndroidToolTypeFinger);
  MotionEventAndroid::Pointer p1(2, -13.7f, 7.13f, 3.5f, 12.1f, -0.1f, 0.4f,
                                 kAndroidToolTypeFinger);
  float raw_offset = -3.f;
  int pointer_count = 2;
  int history_size = 0;
  int action_index = -1;
  MotionEventAndroidJava event(
      base::android::AttachCurrentThread(), nullptr, kPixToDip, 0.f, 0.f, 0.f,
      oldest_event_time, latest_event_time, kAndroidActionDown, pointer_count,
      history_size, action_index, kAndroidActionButton, 0,
      kAndroidButtonPrimary, kAndroidAltKeyDown, 0, raw_offset, -raw_offset,
      false, &p0, &p1);

  EXPECT_EQ(MotionEvent::Action::DOWN, event.GetAction());
  EXPECT_EQ(oldest_event_time, event.GetEventTime());
  EXPECT_EQ(latest_event_time, event.GetLatestEventTime());
  EXPECT_EQ(p0.pos_x_pixels * kPixToDip, event.GetX(0));
  EXPECT_EQ(p0.pos_y_pixels * kPixToDip, event.GetY(0));
  EXPECT_EQ(p1.pos_x_pixels * kPixToDip, event.GetX(1));
  EXPECT_EQ(p1.pos_y_pixels * kPixToDip, event.GetY(1));
  EXPECT_FLOAT_EQ((p0.pos_x_pixels + raw_offset) * kPixToDip, event.GetRawX(0));
  EXPECT_FLOAT_EQ((p0.pos_y_pixels - raw_offset) * kPixToDip, event.GetRawY(0));
  EXPECT_FLOAT_EQ((p1.pos_x_pixels + raw_offset) * kPixToDip, event.GetRawX(1));
  EXPECT_FLOAT_EQ((p1.pos_y_pixels - raw_offset) * kPixToDip, event.GetRawY(1));
  EXPECT_EQ(p0.touch_major_pixels * kPixToDip, event.GetTouchMajor(0));
  EXPECT_EQ(p1.touch_major_pixels * kPixToDip, event.GetTouchMajor(1));
  EXPECT_EQ(p0.touch_minor_pixels * kPixToDip, event.GetTouchMinor(0));
  EXPECT_EQ(p1.touch_minor_pixels * kPixToDip, event.GetTouchMinor(1));
  EXPECT_EQ(p0.orientation_rad, event.GetOrientation(0));
  EXPECT_EQ(p1.orientation_rad, event.GetOrientation(1));
  EXPECT_NEAR(p0.tilt_rad,
              ConvertToTiltRad(event.GetTiltX(0), event.GetTiltY(0)),
              float_error);
  EXPECT_NEAR(p1.tilt_rad,
              ConvertToTiltRad(event.GetTiltX(1), event.GetTiltY(1)),
              float_error);
  EXPECT_EQ(p0.id, event.GetPointerId(0));
  EXPECT_EQ(p1.id, event.GetPointerId(1));
  EXPECT_EQ(MotionEvent::ToolType::FINGER, event.GetToolType(0));
  EXPECT_EQ(MotionEvent::ToolType::FINGER, event.GetToolType(1));
  EXPECT_EQ(MotionEvent::BUTTON_PRIMARY, event.GetButtonState());
  EXPECT_EQ(ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON, event.GetFlags());
  EXPECT_EQ(static_cast<size_t>(pointer_count), event.GetPointerCount());
  EXPECT_EQ(static_cast<size_t>(history_size), event.GetHistorySize());
}

TEST(MotionEventAndroidTest, Clone) {
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks());

  const int pointer_count = 1;
  MotionEventAndroid::Pointer p0(
      1, 13.7f, -7.13f, 5.3f, 1.2f, 0.1f, 0.2f, kAndroidToolTypeFinger);
  MotionEventAndroidJava event(base::android::AttachCurrentThread(), nullptr,
                               kPixToDip, 0, 0, 0, base::TimeTicks(),
                               kAndroidActionDown, pointer_count, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, false, &p0, nullptr);

  std::unique_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(ui::test::ToString(event), ui::test::ToString(*clone));
}

TEST(MotionEventAndroidTest, Cancel) {
  constexpr const int kEventTimeNS = 5'123'456;
  base::TimeTicks event_time =
      base::TimeTicks() + base::Nanoseconds(kEventTimeNS);
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(event_time);

  const int pointer_count = 1;
  MotionEventAndroid::Pointer p0(
      1, 13.7f, -7.13f, 5.3f, 1.2f, 0.1f, 0.2f, kAndroidToolTypeFinger);
  MotionEventAndroidJava event(
      base::android::AttachCurrentThread(), nullptr, kPixToDip, 0, 0, 0,
      base::TimeTicks() + base::Nanoseconds(kEventTimeNS), kAndroidActionDown,
      pointer_count, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, &p0, nullptr);

  std::unique_ptr<MotionEvent> cancel_event = event.Cancel();
  EXPECT_EQ(MotionEvent::Action::CANCEL, cancel_event->GetAction());
  EXPECT_EQ(event_time, cancel_event->GetEventTime());
  EXPECT_EQ(p0.pos_x_pixels * kPixToDip, cancel_event->GetX(0));
  EXPECT_EQ(p0.pos_y_pixels * kPixToDip, cancel_event->GetY(0));
  EXPECT_EQ(static_cast<size_t>(pointer_count),
            cancel_event->GetPointerCount());
  EXPECT_EQ(0U, cancel_event->GetHistorySize());
}

TEST(MotionEventAndroidTest, InvalidOrientationsSanitized) {
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks());

  int pointer_count = 2;
  float orientation0 = 1e10f;
  float orientation1 = std::numeric_limits<float>::quiet_NaN();
  MotionEventAndroid::Pointer p0(0, 0, 0, 0, 0, orientation0, 0, 0);
  MotionEventAndroid::Pointer p1(1, 0, 0, 0, 0, orientation1, 0, 0);
  MotionEventAndroidJava event(base::android::AttachCurrentThread(), nullptr,
                               kPixToDip, 0, 0, 0, base::TimeTicks(),
                               kAndroidActionDown, pointer_count, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, false, &p0, &p1);

  EXPECT_EQ(0.f, event.GetOrientation(0));
  EXPECT_EQ(0.f, event.GetOrientation(1));
}

TEST(MotionEventAndroidTest, NonEmptyHistoryForNonMoveEventsSanitized) {
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks());

  int pointer_count = 1;
  size_t history_size = 5;
  MotionEventAndroid::Pointer p0(0, 0, 0, 0, 0, 0, 0, 0);
  MotionEventAndroidJava event(
      base::android::AttachCurrentThread(), nullptr, kPixToDip, 0, 0, 0,
      base::TimeTicks(), base::TimeTicks(), kAndroidActionDown, pointer_count,
      history_size, 0, 0, 0, 0, 0, 0, 0, 0, false, &p0, nullptr);

  EXPECT_EQ(0U, event.GetHistorySize());
}

TEST(MotionEventAndroidTest, ActionIndexForPointerDown) {
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks());

  MotionEventAndroid::Pointer p0(
      1, 13.7f, -7.13f, 5.3f, 1.2f, 0.1f, 0.2f, kAndroidToolTypeFinger);
  MotionEventAndroid::Pointer p1(
      2, -13.7f, 7.13f, 3.5f, 12.1f, -0.1f, -0.4f, kAndroidToolTypeFinger);
  int pointer_count = 2;
  int history_size = 0;
  int action_index = 1;
  MotionEventAndroidJava event(
      base::android::AttachCurrentThread(), nullptr, kPixToDip, 0, 0, 0,
      base::TimeTicks(), kAndroidActionPointerDown, pointer_count, history_size,
      action_index, 0, 0, 0, 0, 0, 0, 0, false, &p0, &p1);

  EXPECT_EQ(MotionEvent::Action::POINTER_DOWN, event.GetAction());
  EXPECT_EQ(action_index, event.GetActionIndex());
}

}  // namespace ui
