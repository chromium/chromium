// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_source_native.h"

#include <android/input.h>

#include "base/notreached.h"
#include "ui/events/android/events_android_utils.h"

namespace ui {

MotionEventAndroidSourceNative::MotionEventAndroidSourceNative(
    base::android::ScopedInputEvent event,
    float y_offset_pix)
    : event_(std::move(event)), y_offset_pix_(y_offset_pix) {
  CHECK(event_);
}

MotionEventAndroidSourceNative::~MotionEventAndroidSourceNative() = default;

int MotionEventAndroidSourceNative::GetPointerId(size_t pointer_index) const {
  return AMotionEvent_getPointerId(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetXPix(size_t pointer_index) const {
  return AMotionEvent_getX(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetYPix(size_t pointer_index) const {
  return AMotionEvent_getY(event_.a_input_event(), pointer_index) +
         y_offset_pix_;
}

float MotionEventAndroidSourceNative::GetTouchMajorPix(
    size_t pointer_index) const {
  return AMotionEvent_getTouchMajor(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetTouchMinorPix(
    size_t pointer_index) const {
  return AMotionEvent_getTouchMinor(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetRawOrientation(
    size_t pointer_index) const {
  return AMotionEvent_getOrientation(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetPressure(size_t pointer_index) const {
  return AMotionEvent_getPressure(event_.a_input_event(), pointer_index);
}

float MotionEventAndroidSourceNative::GetAxisHscroll(
    size_t pointer_index) const {
  return AMotionEvent_getAxisValue(event_.a_input_event(),
                                   AMOTION_EVENT_AXIS_HSCROLL, pointer_index);
}

float MotionEventAndroidSourceNative::GetAxisVscroll(
    size_t pointer_index) const {
  return AMotionEvent_getAxisValue(event_.a_input_event(),
                                   AMOTION_EVENT_AXIS_VSCROLL, pointer_index);
}

float MotionEventAndroidSourceNative::GetRawTilt(size_t pointer_index) const {
  return AMotionEvent_getAxisValue(event_.a_input_event(),
                                   AMOTION_EVENT_AXIS_TILT, pointer_index);
}

MotionEvent::ToolType MotionEventAndroidSourceNative::GetToolType(
    size_t pointer_index) const {
  return FromAndroidToolType(
      AMotionEvent_getToolType(event_.a_input_event(), pointer_index));
}

int MotionEventAndroidSourceNative::GetActionMasked() const {
  return AMotionEvent_getAction(event_.a_input_event()) &
         AMOTION_EVENT_ACTION_MASK;
}

int MotionEventAndroidSourceNative::GetButtonState() const {
  return AMotionEvent_getButtonState(event_.a_input_event());
}

base::TimeTicks MotionEventAndroidSourceNative::GetHistoricalEventTime(
    size_t historical_index) const {
  return base::TimeTicks::FromJavaNanoTime(AMotionEvent_getHistoricalEventTime(
      event_.a_input_event(), historical_index));
}

float MotionEventAndroidSourceNative::GetHistoricalTouchMajorPix(
    size_t pointer_index,
    size_t historical_index) const {
  return AMotionEvent_getHistoricalTouchMajor(event_.a_input_event(),
                                              pointer_index, historical_index);
}

float MotionEventAndroidSourceNative::GetHistoricalXPix(
    size_t pointer_index,
    size_t historical_index) const {
  return AMotionEvent_getHistoricalX(event_.a_input_event(), pointer_index,
                                     historical_index);
}

float MotionEventAndroidSourceNative::GetHistoricalYPix(
    size_t pointer_index,
    size_t historical_index) const {
  return AMotionEvent_getHistoricalY(event_.a_input_event(), pointer_index,
                                     historical_index) +
         y_offset_pix_;
}

bool MotionEventAndroidSourceNative::IsLatestEventTimeResampled() const {
  return false;
}

int MotionEventAndroidSourceNative::GetSource() const {
  return AInputEvent_getSource(event_.a_input_event());
}

base::android::ScopedJavaLocalRef<jobject>
MotionEventAndroidSourceNative::GetJavaObject() const {
  return nullptr;
}

int MotionEventAndroidSourceNative::GetMetaState() const {
  return AMotionEvent_getMetaState(event_.a_input_event());
}

std::unique_ptr<MotionEventAndroidSource>
MotionEventAndroidSourceNative::Clone() const {
  NOTREACHED();
}

}  // namespace ui
