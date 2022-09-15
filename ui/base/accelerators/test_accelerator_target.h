// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_TEST_ACCELERATOR_TARGET_H_
#define UI_BASE_ACCELERATORS_TEST_ACCELERATOR_TARGET_H_

#include "ui/base/accelerators/accelerator.h"

namespace ui {

// AcceleratorTarget implementation suitable for tests. Tracks calls to
// AcceleratorPressed() and allows for configuration of values returned by
// various functions.
class TestAcceleratorTarget : public AcceleratorTarget {
 public:
  // |accelerator_pressed_result| is used as the return value for
  // AcceleratorPressed().
  explicit TestAcceleratorTarget(bool accelerator_pressed_result = true);

  TestAcceleratorTarget(const TestAcceleratorTarget&) = delete;
  TestAcceleratorTarget& operator=(const TestAcceleratorTarget&) = delete;

  ~TestAcceleratorTarget() override;

  void set_can_handle_accelerators(bool can_handle_accelerators) {
    can_handle_accelerators_ = can_handle_accelerators;
  }

  int accelerator_count() const { return accelerator_count_; }
  int accelerator_repeat_count() const { return accelerator_repeat_count_; }

  // Returns the number of times AcceleratorPressed() was called with an
  // accelerator whose repeat value was false.
  int accelerator_non_repeat_count() const {
    return accelerator_count_ - accelerator_repeat_count_;
  }

  void ResetCounts();

  // AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  // Number of times AcceleratorPressed() was called.
  int accelerator_count_ = 0;

  // Number of times AcceleratorPressed() was called and Accelerator::IsRepeat()
  // was true.
  int accelerator_repeat_count_ = 0;

  // Return value of AcceleratorPressed().
  const bool accelerator_pressed_result_;

  // Return value of CanHandleAccelerators().
  bool can_handle_accelerators_ = true;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_TEST_ACCELERATOR_TARGET_H_
