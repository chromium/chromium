// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_CACHED_HISTORICAL_EVENT_SOURCE_H_
#define UI_EVENTS_ANDROID_CACHED_HISTORICAL_EVENT_SOURCE_H_

#include "ui/events/android/motion_event_android.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/events_export.h"

namespace ui {

namespace mojom {
class HistoricalCachedPointerDataView;
}

// This source is expected to be used with a fully cached version of
// MotionEventAndroid i.e. where `MotionEventAndroid::GetPointerCount() ==
// MotionEventAndroid::cached_pointers_.size()`
// The object also is able to cache the values that aren't cacheable by
// MotionEventAndroid like historical events.
class EVENTS_EXPORT CachedHistoricalEventSource
    : public MotionEventAndroidSource {
 public:
  static std::unique_ptr<MotionEventAndroidSource> Create(
      const base::android::JavaRef<jobject>& event,
      bool is_latest_event_time_resampled);

  CachedHistoricalEventSource(const CachedHistoricalEventSource&) = delete;
  CachedHistoricalEventSource& operator=(const CachedHistoricalEventSource&) =
      delete;

  CachedHistoricalEventSource();
  ~CachedHistoricalEventSource() override;

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

  struct HistoricalCachedPointer {
    HistoricalCachedPointer();
    ~HistoricalCachedPointer();
    HistoricalCachedPointer(const HistoricalCachedPointer&);
    HistoricalCachedPointer& operator=(const HistoricalCachedPointer&);

    std::vector<MotionEventAndroid::PointerCoordinates> pointers;
    base::TimeTicks event_time;
  };

 private:
  friend struct mojo::StructTraits<ui::mojom::CachedMotionEventAndroidDataView,
                                   std::unique_ptr<ui::MotionEventAndroid>>;
  friend struct mojo::StructTraits<
      ui::mojom::HistoricalCachedPointerDataView,
      ui::CachedHistoricalEventSource::HistoricalCachedPointer>;
  std::vector<HistoricalCachedPointer> historical_events_;
  float pix_to_dip_;
  int input_source_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_CACHED_HISTORICAL_EVENT_SOURCE_H_
