// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_BASE_MOVING_MAX_H_
#define UI_BASE_MOVING_MAX_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/component_export.h"

namespace ui {

// Simple class to efficiently calculate maximum in a moving window.
// This class isn't thread safe.
class COMPONENT_EXPORT(UI_BASE) MovingMax {
 public:
  explicit MovingMax(size_t window_size);
  ~MovingMax();
  // Add new sample to the stream.
  void Put(int value);
  // Get the maximum of the last `window_size` elements.
  int Max() const;

 private:
  const size_t window_size_;
  // Circular buffer with some values in the window.
  // Only possible candidates for maximum are stored:
  // values form a non-increasing sequence.
  std::vector<int> values_;
  // Circular buffer storing when numbers in `values_` were added.
  std::vector<size_t> added_at_;
  // Begin of the circular buffers above.
  size_t begin_idx_ = 0;
  // How many elements are stored in the circular buffers above.
  size_t size_ = 0;
  // Counter of all `Put` operations.
  size_t total_added_ = 0;
};

}  // namespace ui
#endif  // UI_BASE_MOVING_MAX_H_
