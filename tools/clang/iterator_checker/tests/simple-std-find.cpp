// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void IteratorCheckedAfterFind(const std::vector<int>& v) {
  auto it = std::find(std::begin(v), std::end(v), 3);
  if (it != std::end(v)) {
    // Fine because it was checked against `end`.
    *it;
  }
}

void IteratorCheckedAfterFindThenErased(std::vector<int> v) {
  auto it = std::find(std::begin(v), std::end(v), 3);
  if (it != std::end(v)) {
    v.erase(it);
  }
  // Invalid because we might have entered the `if` block and invalidated this
  // iterator.
  *it;
}
