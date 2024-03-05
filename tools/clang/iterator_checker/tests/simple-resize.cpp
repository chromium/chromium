// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void IteratorInvalidAfterResize(std::vector<int>& v, int new_size) {
  auto it = std::begin(v);

  v.resize(new_size);

  // Invalid because resize might have invalidated this iterator.
  *it;
}

void IteratorValidAfterResize(std::vector<int>& v, int new_size) {
  auto it = std::begin(v);

  v.resize(new_size);

  it = std::begin(v);

  if (it != std::end(v)) {
    // Fine because it was reassigned after `resize` and checked against `end`.
    *it;
  }
}
