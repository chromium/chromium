// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dual_gpu_state.h"

#include "base/containers/flat_set.h"
#include "base/trace_event/trace_event.h"

namespace gl {

class GLContext;

DualGPUState::DualGPUState() {}

DualGPUState::~DualGPUState() {}

void DualGPUState::RegisterHighPerformanceContext(GLContext* context) {
  if (contexts_.contains(context))
    return;

  CancelDelayedSwitchToLowPowerGPU();
  contexts_.insert(context);
  SwitchToHighPerformanceGPUIfNeeded();
}

void DualGPUState::RemoveHighPerformanceContext(GLContext* context) {
  if (!contexts_.contains(context))
    return;

  contexts_.erase(context);
  if (contexts_.empty())
    AttemptSwitchToLowPowerGPUWithDelay();
}

}  // namespace gl
