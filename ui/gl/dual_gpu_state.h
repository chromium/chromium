// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DUAL_GPU_STATE_H_
#define UI_GL_DUAL_GPU_STATE_H_

#include "base/containers/flat_set.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLContext;

class GL_EXPORT DualGPUState {
 public:
  DualGPUState(const DualGPUState&) = delete;
  DualGPUState& operator=(const DualGPUState&) = delete;

  void RegisterHighPerformanceContext(GLContext* context);
  void RemoveHighPerformanceContext(GLContext* context);

 protected:
  DualGPUState();
  ~DualGPUState();

 private:
  virtual void SwitchToHighPerformanceGPUIfNeeded() = 0;
  virtual void SwitchToLowPowerGPU() = 0;
  virtual void AttemptSwitchToLowPowerGPUWithDelay() = 0;
  virtual void CancelDelayedSwitchToLowPowerGPU() = 0;

  base::flat_set<GLContext*> contexts_;
};

}  // namespace gl

#endif  // UI_GL_DUAL_GPU_STATE_H_
