// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/test_accelerator_target.h"

namespace ui {

TestAcceleratorTarget::TestAcceleratorTarget(bool accelerator_pressed_result)
    : accelerator_pressed_result_(accelerator_pressed_result) {}

TestAcceleratorTarget::~TestAcceleratorTarget() = default;

void TestAcceleratorTarget::ResetCounts() {
  accelerator_repeat_count_ = accelerator_count_ = 0;
}

bool TestAcceleratorTarget::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  ++accelerator_count_;
  if (accelerator.IsRepeat())
    accelerator_repeat_count_++;
  return accelerator_pressed_result_;
}

bool TestAcceleratorTarget::CanHandleAccelerators() const {
  return can_handle_accelerators_;
}

}  // namespace ui
