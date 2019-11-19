// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dual_gpu_state_mac.h"

#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

namespace {

const unsigned int kDelayLengthSeconds = 10;

}  // namespace

// static
DualGPUStateMac* DualGPUStateMac::GetInstance() {
  static base::NoDestructor<DualGPUStateMac> instance;
  return instance.get();
}

DualGPUStateMac::DualGPUStateMac() {}

DualGPUStateMac::~DualGPUStateMac() {}

void DualGPUStateMac::SwitchToHighPerformanceGPUIfNeeded() {
  AllocateDiscretePixelFormatObjectIfNeeded();
}

// This function is called by a delayed task posted in
// DualGPUStateMac::AttemptSwitchToLowPowerGPUWithDelay.
void DualGPUStateMac::SwitchToLowPowerGPU() {
  ReleaseDiscretePixelFormatObjectIfNeeded();
}

void DualGPUStateMac::AttemptSwitchToLowPowerGPUWithDelay() {
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    cancelable_delay_callback_.Reset(base::BindOnce(
        []() { DualGPUStateMac::GetInstance()->SwitchToLowPowerGPU(); }));

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, cancelable_delay_callback_.callback(),
        base::TimeDelta::FromSeconds(kDelayLengthSeconds));
  } else {
    SwitchToLowPowerGPU();
  }
}

void DualGPUStateMac::CancelDelayedSwitchToLowPowerGPU() {
  cancelable_delay_callback_.Cancel();
}

void DualGPUStateMac::AllocateDiscretePixelFormatObjectIfNeeded() {
  if (discrete_pixelformat_)
    return;
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &discrete_pixelformat_, &num_pixel_formats);
}

void DualGPUStateMac::ReleaseDiscretePixelFormatObjectIfNeeded() {
  if (!discrete_pixelformat_)
    return;
  CGLReleasePixelFormat(discrete_pixelformat_);
  discrete_pixelformat_ = nullptr;
}

}  // namespace gl
