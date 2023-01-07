// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_SWITCHING_OBSERVER_H_
#define UI_GL_GPU_SWITCHING_OBSERVER_H_

#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"

namespace ui {

class GL_EXPORT GpuSwitchingObserver {
 public:
  virtual ~GpuSwitchingObserver() = default;

  // Called for any observer when the system switches to a different GPU.
  virtual void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {}

  // Called for any observer when a monitor is plugged in.
  virtual void OnDisplayAdded() {}

  // Called for any observer when a monitor is unplugged.
  virtual void OnDisplayRemoved() {}

  // Called for any observer when the display metrics changed.
  virtual void OnDisplayMetricsChanged() {}
};

}  // namespace ui

#endif  // UI_GL_GPU_SWITCHING_OBSERVER_H_
