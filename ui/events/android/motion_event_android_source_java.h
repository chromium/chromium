// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_JAVA_H_
#define UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_JAVA_H_

#include "base/android/scoped_java_ref.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/events_export.h"

namespace ui {

// MotionEventAndroidSource implementation for Java-backed MotionEvents.
class EVENTS_EXPORT MotionEventAndroidSourceJava
    : public MotionEventAndroidSource {
 public:
  static std::unique_ptr<MotionEventAndroidSource> Create(
      const base::android::JavaRef<jobject>& event,
      bool is_latest_event_time_resampled);

  MotionEventAndroidSourceJava(const MotionEventAndroidSourceJava&) = delete;
  MotionEventAndroidSourceJava& operator=(const MotionEventAndroidSourceJava&) =
      delete;

  ~MotionEventAndroidSourceJava() override;

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
  explicit MotionEventAndroidSourceJava(
      const base::android::JavaRef<jobject>& event,
      bool is_latest_event_time_resampled);

  base::android::ScopedJavaGlobalRef<jobject> event_;
  const bool is_latest_event_time_resampled_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_MOTION_EVENT_ANDROID_SOURCE_JAVA_H_
