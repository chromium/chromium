// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

bool Bool();

void IteratorInvalidationInAForLoop(std::vector<int>& v) {
  for (auto it = std::begin(v); it != std::end(v); ++it) {
    if (Bool()) {
      // Calling `erase` invalidates `it`, and the next loop iteration will use
      // it via `++it`
      v.erase(it);
    }
  }
}

void IteratorInvalidationInAWhileLoop(std::vector<int>& v) {
  auto it = std::begin(v);

  while (it != std::end(v)) {
    if (Bool()) {
      // Calling `erase` invalidates `it`, and the next loop iteration will use
      // it via `++it`
      v.erase(it);
    }
    ++it;
  }
}

void IteratorInvalidationInAForeachLoop(std::vector<int>& v) {
  // This is just a syntactic shorthand and uses iterator under the hood
  for (int& x : v) {
    if (x % 2 == 0) {
      // We're invalidating the iterator here
      v.erase(std::remove(v.begin(), v.end(), x), v.end());
    }
  }
}
