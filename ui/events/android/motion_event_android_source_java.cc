// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_source_java.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "ui/events/android/events_android_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ui {

// static
std::unique_ptr<MotionEventAndroidSource> MotionEventAndroidSourceJava::Create(
    const base::android::JavaRef<jobject>& event,
    bool is_latest_event_time_resampled) {
  return base::WrapUnique(
      new MotionEventAndroidSourceJava(event, is_latest_event_time_resampled));
}

MotionEventAndroidSourceJava::MotionEventAndroidSourceJava(
    const base::android::JavaRef<jobject>& event,
    bool is_latest_event_time_resampled)
    : event_(event),
      is_latest_event_time_resampled_(is_latest_event_time_resampled) {
  CHECK(!event_.is_null());
}

MotionEventAndroidSourceJava::~MotionEventAndroidSourceJava() = default;

int MotionEventAndroidSourceJava::GetPointerId(size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getPointerId(AttachCurrentThread(),
                                                        event_, pointer_index);
}

float MotionEventAndroidSourceJava::GetXPix(size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getX(AttachCurrentThread(), event_,
                                                pointer_index);
}

float MotionEventAndroidSourceJava::GetYPix(size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getY(AttachCurrentThread(), event_,
                                                pointer_index);
}

float MotionEventAndroidSourceJava::GetTouchMajorPix(
    size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getTouchMajor(AttachCurrentThread(),
                                                         event_, pointer_index);
}

float MotionEventAndroidSourceJava::GetTouchMinorPix(
    size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getTouchMinor(AttachCurrentThread(),
                                                         event_, pointer_index);
}

float MotionEventAndroidSourceJava::GetRawOrientation(
    size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getOrientation(
      AttachCurrentThread(), event_, pointer_index);
}

float MotionEventAndroidSourceJava::GetPressure(size_t pointer_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getPressure(AttachCurrentThread(),
                                                       event_, pointer_index);
}

float MotionEventAndroidSourceJava::GetAxisHscroll(size_t pointer_index) const {
  return Java_MotionEvent_getAxisValue(AttachCurrentThread(), event_,
                                       JNI_MotionEvent::AXIS_HSCROLL,
                                       pointer_index);
}

float MotionEventAndroidSourceJava::GetAxisVscroll(size_t pointer_index) const {
  return Java_MotionEvent_getAxisValue(AttachCurrentThread(), event_,
                                       JNI_MotionEvent::AXIS_VSCROLL,
                                       pointer_index);
}

float MotionEventAndroidSourceJava::GetRawTilt(size_t pointer_index) const {
  return Java_MotionEvent_getAxisValue(
      AttachCurrentThread(), event_, JNI_MotionEvent::AXIS_TILT, pointer_index);
}

MotionEvent::ToolType MotionEventAndroidSourceJava::GetToolType(
    size_t pointer_index) const {
  return FromAndroidToolType(JNI_MotionEvent::Java_MotionEvent_getToolType(
      AttachCurrentThread(), event_, pointer_index));
}

int MotionEventAndroidSourceJava::GetActionMasked() const {
  return JNI_MotionEvent::Java_MotionEvent_getActionMasked(
      AttachCurrentThread(), event_);
}

int MotionEventAndroidSourceJava::GetButtonState() const {
  return JNI_MotionEvent::Java_MotionEvent_getButtonState(AttachCurrentThread(),
                                                          event_);
}

base::TimeTicks MotionEventAndroidSourceJava::GetHistoricalEventTime(
    size_t historical_index) const {
  jlong time_ms = JNI_MotionEvent::Java_MotionEvent_getHistoricalEventTime(
      AttachCurrentThread(), event_, historical_index);
  return base::TimeTicks::FromUptimeMillis(time_ms);
}

float MotionEventAndroidSourceJava::GetHistoricalTouchMajorPix(
    size_t pointer_index,
    size_t historical_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getHistoricalTouchMajor(
      AttachCurrentThread(), event_, pointer_index, historical_index);
}

float MotionEventAndroidSourceJava::GetHistoricalXPix(
    size_t pointer_index,
    size_t historical_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getHistoricalX(
      AttachCurrentThread(), event_, pointer_index, historical_index);
}

float MotionEventAndroidSourceJava::GetHistoricalYPix(
    size_t pointer_index,
    size_t historical_index) const {
  return JNI_MotionEvent::Java_MotionEvent_getHistoricalY(
      AttachCurrentThread(), event_, pointer_index, historical_index);
}

bool MotionEventAndroidSourceJava::IsLatestEventTimeResampled() const {
  return is_latest_event_time_resampled_;
}

int MotionEventAndroidSourceJava::GetSource() const {
  return JNI_MotionEvent::Java_MotionEvent_getSource(AttachCurrentThread(),
                                                     event_);
}

ScopedJavaLocalRef<jobject> MotionEventAndroidSourceJava::GetJavaObject()
    const {
  return ScopedJavaLocalRef<jobject>(event_);
}

int MotionEventAndroidSourceJava::GetMetaState() const {
  return JNI_MotionEvent::Java_MotionEvent_getMetaState(AttachCurrentThread(),
                                                        event_);
}

std::unique_ptr<MotionEventAndroidSource> MotionEventAndroidSourceJava::Clone()
    const {
  return Create(event_, is_latest_event_time_resampled_);
}

}  // namespace ui
