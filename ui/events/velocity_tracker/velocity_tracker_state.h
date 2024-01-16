// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_VELOCITY_TRACKER_VELOCITY_TRACKER_STATE_H_
#define UI_EVENTS_VELOCITY_TRACKER_VELOCITY_TRACKER_STATE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "ui/events/velocity_tracker/bitset_32.h"
#include "ui/events/velocity_tracker/velocity_tracker.h"

namespace ui {

class MotionEvent;

// Port of VelocityTrackerState from Android
// * platform/frameworks/base/core/jni/android_view_VelocityTracker.cpp
// * Change-Id: I3517881b87b47dcc209d80dbd0ac6b5cf29a766f
// * Please update the Change-Id as upstream Android changes are pulled.
class COMPONENT_EXPORT(VELOCITY_TRACKER) VelocityTrackerState {
 public:
  explicit VelocityTrackerState(VelocityTracker::Strategy strategy);

  VelocityTrackerState(const VelocityTrackerState&) = delete;
  VelocityTrackerState& operator=(const VelocityTrackerState&) = delete;

  ~VelocityTrackerState();

  void Clear();
  void AddMovement(const MotionEvent& event);
  void ComputeCurrentVelocity(int32_t units, float max_velocity);
  float GetXVelocity(int32_t id) const;
  float GetYVelocity(int32_t id) const;

 private:
  struct Velocity {
    float vx, vy;
  };

  void GetVelocity(int32_t id, float* out_vx, float* out_vy) const;

  VelocityTracker velocity_tracker_;
  int32_t active_pointer_id_;
  BitSet32 calculated_id_bits_;
  Velocity calculated_velocity_[VelocityTracker::MAX_POINTERS];
};

}  // namespace ui

#endif  // UI_EVENTS_VELOCITY_TRACKER_VELOCITY_TRACKER_STATE_H_
