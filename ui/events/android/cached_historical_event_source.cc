// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/cached_historical_event_source.h"

#include "base/notreached.h"

namespace ui {

CachedHistoricalEventSource::HistoricalCachedPointer::
    HistoricalCachedPointer() = default;
CachedHistoricalEventSource::HistoricalCachedPointer::
    ~HistoricalCachedPointer() = default;
CachedHistoricalEventSource::HistoricalCachedPointer::HistoricalCachedPointer(
    const HistoricalCachedPointer&) = default;
CachedHistoricalEventSource::HistoricalCachedPointer&
CachedHistoricalEventSource::HistoricalCachedPointer::operator=(
    const HistoricalCachedPointer&) = default;

CachedHistoricalEventSource::CachedHistoricalEventSource() = default;
CachedHistoricalEventSource::~CachedHistoricalEventSource() = default;

int CachedHistoricalEventSource::GetPointerId(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetXPix(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetYPix(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetTouchMajorPix(
    size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetTouchMinorPix(
    size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetRawOrientation(
    size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetPressure(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetAxisHscroll(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetAxisVscroll(size_t pointer_index) const {
  NOTREACHED();
}

float CachedHistoricalEventSource::GetRawTilt(size_t pointer_index) const {
  NOTREACHED();
}

MotionEvent::ToolType CachedHistoricalEventSource::GetToolType(
    size_t pointer_index) const {
  NOTREACHED();
}
int CachedHistoricalEventSource::GetActionMasked() const {
  NOTREACHED();
}
int CachedHistoricalEventSource::GetButtonState() const {
  NOTREACHED();
}

base::TimeTicks CachedHistoricalEventSource::GetHistoricalEventTime(
    size_t historical_index) const {
  return historical_events_[historical_index].event_time;
}

float CachedHistoricalEventSource::GetHistoricalTouchMajorPix(
    size_t pointer_index,
    size_t historical_index) const {
  return historical_events_[historical_index]
             .pointers[pointer_index]
             .touch_major /
         pix_to_dip_;
}

float CachedHistoricalEventSource::GetHistoricalXPix(
    size_t pointer_index,
    size_t historical_index) const {
  return historical_events_[historical_index]
             .pointers[pointer_index]
             .position.x() /
         pix_to_dip_;
}

float CachedHistoricalEventSource::GetHistoricalYPix(
    size_t pointer_index,
    size_t historical_index) const {
  return historical_events_[historical_index]
             .pointers[pointer_index]
             .position.y() /
         pix_to_dip_;
}

bool CachedHistoricalEventSource::IsLatestEventTimeResampled() const {
  NOTREACHED();
}

int CachedHistoricalEventSource::GetSource() const {
  return input_source_;
}

base::android::ScopedJavaLocalRef<jobject>
CachedHistoricalEventSource::GetJavaObject() const {
  return nullptr;
}

int CachedHistoricalEventSource::GetMetaState() const {
  NOTREACHED();
}

std::unique_ptr<MotionEventAndroidSource> CachedHistoricalEventSource::Clone()
    const {
  NOTREACHED();
}

}  // namespace ui
