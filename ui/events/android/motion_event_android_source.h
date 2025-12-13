// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_H_

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace ui {

// An interface for classes that can provide data from an Android MotionEvent.
// This is used to abstract the underlying data source (e.g., a Java MotionEvent
// or a native MotionEvent) from the MotionEventAndroid class.
// All methods in this interface should return raw, unconverted values.
class EVENTS_EXPORT MotionEventAndroidSource {
 public:
  virtual ~MotionEventAndroidSource() = default;

  virtual int GetPointerId(size_t pointer_index) const = 0;
  virtual float GetXPix(size_t pointer_index) const = 0;
  virtual float GetYPix(size_t pointer_index) const = 0;
  virtual float GetTouchMajorPix(size_t pointer_index) const = 0;
  virtual float GetTouchMinorPix(size_t pointer_index) const = 0;
  virtual float GetRawOrientation(size_t pointer_index) const = 0;
  virtual float GetPressure(size_t pointer_index) const = 0;
  virtual float GetAxisHscroll(size_t pointer_index) const = 0;
  virtual float GetAxisVscroll(size_t pointer_index) const = 0;
  virtual float GetRawTilt(size_t pointer_index) const = 0;
  virtual MotionEvent::ToolType GetToolType(size_t pointer_index) const = 0;
  virtual int GetActionMasked() const = 0;
  virtual int GetButtonState() const = 0;
  virtual base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const = 0;
  virtual float GetHistoricalTouchMajorPix(size_t pointer_index,
                                           size_t historical_index) const = 0;
  virtual float GetHistoricalXPix(size_t pointer_index,
                                  size_t historical_index) const = 0;
  virtual float GetHistoricalYPix(size_t pointer_index,
                                  size_t historical_index) const = 0;

  // This is specific to Java-backed events. Native-backed events should
  // return false.
  virtual bool IsLatestEventTimeResampled() const = 0;

  virtual int GetSource() const = 0;

  // This is specific to Java-backed events. Native-backed events should
  // return nullptr.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const = 0;
  virtual int GetMetaState() const = 0;

  virtual std::unique_ptr<MotionEventAndroidSource> Clone() const = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_H_
