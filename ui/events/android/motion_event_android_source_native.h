// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_NATIVE_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_NATIVE_H_

#include <android/input.h>

#include "base/android/scoped_input_event.h"
#include "ui/events/android/motion_event_android_source.h"

namespace ui {

// MotionEventAndroidSource implementation for native C++ MotionEvents.
class MotionEventAndroidSourceNative : public MotionEventAndroidSource {
 public:
  MotionEventAndroidSourceNative(base::android::ScopedInputEvent event,
                                 float y_offset_pix);

  MotionEventAndroidSourceNative(const MotionEventAndroidSourceNative&) =
      delete;
  MotionEventAndroidSourceNative& operator=(
      const MotionEventAndroidSourceNative&) = delete;

  ~MotionEventAndroidSourceNative() override;

  // MotionEventAndroidSource:
  int GetPointerId(size_t pointer_index) const override;
  float GetXPix(size_t pointer_index) const override;
  float GetYPix(size_t pointer_index) const override;
  float GetTouchMajorPix(size_t pointer_index) const override;
  float GetTouchMinorPix(size_t pointer_index) const override;
  float GetRawOrientation(size_t pointer_index) const override;
  float GetPressure(size_t pointer_index) const override;
  float GetAxisHscroll(size_t pointer_index) const override;
  float GetAxisVscroll(size_t pointer_index) const override;
  float GetRawTilt(size_t pointer_index) const override;
  MotionEvent::ToolType GetToolType(size_t pointer_index) const override;
  int GetActionMasked() const override;
  int GetButtonState() const override;
  base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const override;
  float GetHistoricalTouchMajorPix(size_t pointer_index,
                                   size_t historical_index) const override;
  float GetHistoricalXPix(size_t pointer_index,
                          size_t historical_index) const override;
  float GetHistoricalYPix(size_t pointer_index,
                          size_t historical_index) const override;
  bool IsLatestEventTimeResampled() const override;
  int GetSource() const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  int GetMetaState() const override;
  std::unique_ptr<MotionEventAndroidSource> Clone() const override;

 private:
  const base::android::ScopedInputEvent event_;
  const float y_offset_pix_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_NATIVE_H_
