// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DUAL_GPU_STATE_MAC_H_
#define UI_GL_DUAL_GPU_STATE_MAC_H_

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gl/dual_gpu_state.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT DualGPUStateMac : public DualGPUState {
 public:
  static DualGPUStateMac* GetInstance();

 private:
  friend base::NoDestructor<DualGPUStateMac>;

  DualGPUStateMac();
  ~DualGPUStateMac();

  void SwitchToHighPerformanceGPUIfNeeded() override;
  void SwitchToLowPowerGPU() override;
  void AttemptSwitchToLowPowerGPUWithDelay() override;
  void CancelDelayedSwitchToLowPowerGPU() override;

  void AllocateDiscretePixelFormatObjectIfNeeded();
  void ReleaseDiscretePixelFormatObjectIfNeeded();

  CGLPixelFormatObj discrete_pixelformat_ = nullptr;
  base::CancelableOnceClosure cancelable_delay_callback_;

  DISALLOW_COPY_AND_ASSIGN(DualGPUStateMac);
};

}  // namespace gl

#endif  // UI_GL_DUAL_GPU_STATE_MAC_H_
