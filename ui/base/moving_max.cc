// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/moving_max.h"

#include <base/check_op.h>

namespace ui {

MovingMax::MovingMax(size_t window_size)
    : window_size_(window_size), values_(window_size), added_at_(window_size) {}

MovingMax::~MovingMax() = default;

void MovingMax::Put(int value) {
  ++total_added_;
  // Remove old elements from the back of the window;
  while (size_ > 0 && added_at_[begin_idx_] + window_size_ <= total_added_) {
    begin_idx_ = (begin_idx_ + 1) % window_size_;
    --size_;
  }
  // Remove small elements from the front of the window because they can never
  // become the maximum in the window since the currently added element is
  // bigger than them and will leave the window later.
  while (size_ > 0 &&
         values_[(begin_idx_ + size_ - 1) % window_size_] < value) {
    --size_;
  }
  DCHECK_LT(size_, window_size_);
  values_[(begin_idx_ + size_) % window_size_] = value;
  added_at_[(begin_idx_ + size_) % window_size_] = total_added_;
  ++size_;
}

int MovingMax::Max() const {
  DCHECK_GT(size_, 0u);
  return values_[begin_idx_];
}
}  // namespace ui
