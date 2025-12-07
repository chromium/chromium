// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojom/motion_event_android_mojom_traits.h"

#include <android/input.h>

#include "base/android/jni_android.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/mojom/motion_event_android.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/motion_event_test_utils_jni_headers/MotionEventTestUtils_jni.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace ui {

namespace {

std::unique_ptr<MotionEventAndroid> GetMotionEvent() {
  const base::TimeTicks down_time = base::TimeTicks::FromUptimeMillis(
      (base::TimeTicks::Now() - base::Milliseconds(100)).ToUptimeMillis());
  const base::TimeTicks oldest_event_time = down_time + base::Milliseconds(8);
  const base::TimeTicks latest_event_time = down_time + base::Milliseconds(16);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_motion_event =
      Java_MotionEventTestUtils_getMultiTouchEventWithHistory(
          env, oldest_event_time.ToUptimeMillis(), down_time.ToUptimeMillis(),
          latest_event_time.ToUptimeMillis());

  MotionEventAndroid::Pointer p0(
      /*id=*/JNI_MotionEvent::Java_MotionEvent_getPointerId(
          env, java_motion_event, 0),
      /*pos_x_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getX(env, java_motion_event, 0),
      /*pos_y_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getY(env, java_motion_event, 0),
      /*touch_major_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getTouchMajor(env, java_motion_event,
                                                      0),
      /*touch_minor_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getTouchMinor(env, java_motion_event,
                                                      0),
      /*pressure=*/
      JNI_MotionEvent::Java_MotionEvent_getPressure(env, java_motion_event, 0),
      /*orientation_rad=*/0.1f,
      /*tilt_rad=*/0.2f,
      /*tool_type=*/AMOTION_EVENT_TOOL_TYPE_FINGER);
  MotionEventAndroid::Pointer p1(
      /*id=*/JNI_MotionEvent::Java_MotionEvent_getPointerId(
          env, java_motion_event, 1),
      /*pos_x_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getX(env, java_motion_event, 1),
      /*pos_y_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getY(env, java_motion_event, 1),
      /*touch_major_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getTouchMajor(env, java_motion_event,
                                                      0),
      /*touch_minor_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getTouchMinor(env, java_motion_event,
                                                      0),
      /*pressure=*/
      JNI_MotionEvent::Java_MotionEvent_getPressure(env, java_motion_event, 0),
      /*orientation_rad=*/0.1f,
      /*tilt_rad=*/0.2f,
      /*tool_type=*/AMOTION_EVENT_TOOL_TYPE_FINGER);

  constexpr float kPixToDip = 0.5f;
  auto event = ui::MotionEventAndroidFactory::CreateFromJava(
      base::android::AttachCurrentThread(), java_motion_event, kPixToDip, 0, 0,
      0, oldest_event_time, latest_event_time, down_time,
      AMOTION_EVENT_ACTION_MOVE,
      /*pointer_count=*/
      JNI_MotionEvent::Java_MotionEvent_getPointerCount(env, java_motion_event),
      /*history_size=*/
      JNI_MotionEvent::Java_MotionEvent_getHistorySize(env, java_motion_event),
      0, 0, 0, 0,
      /*raw_offset_x_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getRawX(env, java_motion_event) -
          JNI_MotionEvent::Java_MotionEvent_getX(env, java_motion_event, 0),
      /*raw_offset_y_pixels=*/
      JNI_MotionEvent::Java_MotionEvent_getRawY(env, java_motion_event) -
          JNI_MotionEvent::Java_MotionEvent_getY(env, java_motion_event, 0),
      false, &p0, &p1, false);

  return event;
}

}  // namespace

TEST(MotionEventAndroidTraitsTest, MultiplePointersWithHistory) {
  std::unique_ptr<ui::MotionEventAndroid> expected_event = GetMotionEvent();

  ASSERT_GT(expected_event->GetHistorySize(), 0u);
  // Trying to serialize a motion event with more than
  // `MotionEventAndroid::kDefaultCachedPointers` which is set to 2.
  ASSERT_GT(expected_event->GetPointerCount(),
            MotionEventAndroid::kDefaultCachedPointers);

  std::unique_ptr<MotionEventAndroid> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::CachedMotionEventAndroid>(
          expected_event, output));

  EXPECT_EQ(expected_event->GetSource(), output->GetSource());
  EXPECT_EQ(expected_event->GetAction(), output->GetAction());
  ASSERT_EQ(expected_event->GetPointerCount(), output->GetPointerCount());
  for (size_t ind = 0; ind < expected_event->GetPointerCount(); ind++) {
    EXPECT_EQ(expected_event->GetX(ind), output->GetX(ind));
    EXPECT_EQ(expected_event->GetXPix(ind), output->GetXPix(ind));
    EXPECT_EQ(expected_event->GetRawX(ind), output->GetRawX(ind));
    EXPECT_EQ(expected_event->GetY(ind), output->GetY(ind));
    EXPECT_EQ(expected_event->GetYPix(ind), output->GetYPix(ind));
    EXPECT_EQ(expected_event->GetRawY(ind), output->GetRawY(ind));
    EXPECT_EQ(expected_event->GetPointerId(ind), output->GetPointerId(ind));
    EXPECT_EQ(expected_event->GetTouchMajor(ind), output->GetTouchMajor(ind));
    EXPECT_EQ(expected_event->GetTouchMinor(ind), output->GetTouchMinor(ind));
    EXPECT_EQ(expected_event->GetOrientation(ind), output->GetOrientation(ind));
    EXPECT_EQ(expected_event->GetPressure(ind), output->GetPressure(ind));
    EXPECT_EQ(expected_event->GetTiltX(ind), output->GetTiltX(ind));
    EXPECT_EQ(expected_event->GetTiltY(ind), output->GetTiltY(ind));
    EXPECT_EQ(expected_event->GetTwist(ind), output->GetTwist(ind));
    EXPECT_EQ(expected_event->GetTangentialPressure(ind),
              output->GetTangentialPressure(ind));
    EXPECT_EQ(expected_event->GetToolType(ind), output->GetToolType(ind));
  }

  ASSERT_EQ(expected_event->GetHistorySize(), output->GetHistorySize());
  for (size_t history_ind = 0; history_ind < expected_event->GetHistorySize();
       history_ind++) {
    EXPECT_EQ(expected_event->GetHistoricalEventTime(history_ind),
              output->GetHistoricalEventTime(history_ind));
    for (size_t pointer_ind = 0;
         pointer_ind < expected_event->GetPointerCount(); pointer_ind++) {
      EXPECT_EQ(
          expected_event->GetHistoricalTouchMajor(pointer_ind, history_ind),
          output->GetHistoricalTouchMajor(pointer_ind, history_ind));
      EXPECT_EQ(expected_event->GetHistoricalX(pointer_ind, history_ind),
                output->GetHistoricalX(pointer_ind, history_ind));
      EXPECT_EQ(expected_event->GetHistoricalY(pointer_ind, history_ind),
                output->GetHistoricalY(pointer_ind, history_ind));
    }
  }
}

TEST(MotionEventAndroidTraitsTest, NoPointers) {
  ui::mojom::CachedMotionEventAndroidPtr input;
  {
    const std::unique_ptr<ui::MotionEventAndroid> event = GetMotionEvent();
    auto data = ui::mojom::CachedMotionEventAndroid::Serialize(&event);
    ASSERT_TRUE(ui::mojom::CachedMotionEventAndroid::Deserialize(
        std::move(data), &input));
  }

  input->pointers.clear();

  auto data = ui::mojom::CachedMotionEventAndroid::Serialize(&input);
  std::unique_ptr<MotionEventAndroid> output;
  EXPECT_FALSE(ui::mojom::CachedMotionEventAndroid::Deserialize(std::move(data),
                                                                &output));
}

TEST(MotionEventAndroidTraitsTest, PointerCountMismatch) {
  const std::unique_ptr<ui::MotionEventAndroid> event = GetMotionEvent();
  ui::mojom::CachedMotionEventAndroidPtr input;
  auto data = ui::mojom::CachedMotionEventAndroid::Serialize(&event);
  ASSERT_TRUE(ui::mojom::CachedMotionEventAndroid::Deserialize(std::move(data),
                                                               &input));
  ASSERT_GE(input->pointers.size(), 2u);
  for (auto& historical_data : input->historical_events) {
    ASSERT_GE(input->pointers.size(), 2u);
    historical_data.pointers.pop_back();
  }
  std::unique_ptr<MotionEventAndroid> output;
  EXPECT_FALSE(ui::mojom::CachedMotionEventAndroid::Deserialize(
      ui::mojom::CachedMotionEventAndroid::Serialize(&input), &output));
}

TEST(CachedPointerTraitsTest, ToolType) {
  ui::mojom::MotionEventAndroidCachedPointerPtr input =
      ui::mojom::MotionEventAndroidCachedPointer::New();

  int max_valid_tool_type = static_cast<int>(ui::MotionEvent::ToolType::LAST);
  for (int i = -1; i <= max_valid_tool_type + 1; i++) {
    bool expected_success = i >= 0 && i <= max_valid_tool_type;
    input->tool_type = i;
    auto data = ui::mojom::MotionEventAndroidCachedPointer::Serialize(&input);
    ui::MotionEventAndroid::CachedPointer output;
    EXPECT_EQ(ui::mojom::MotionEventAndroidCachedPointer::Deserialize(
                  std::move(data), &output),
              expected_success);
  }
}

}  // namespace ui

DEFINE_JNI(MotionEventTestUtils)
