// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

void IteratorUsedAfterErase(std::vector<int>& v) {
  auto it = std::begin(v);
  for (; it != std::end(v); ++it) {
    // Note that this access is valid, because we theoretically always check it
    // against `end` before going here.
    if (*it > 3) {
      // Calling `erase` invalidates `it`, and the next loop iteration will use
      // it via `++it`.
      // To fix this error:
      // it = v.erase(it);
      v.erase(it);
    }
  }
}

void IteratorUsedAfterPushBack(std::vector<int>& v) {
  auto it = std::begin(v);
  // Note that `*it == 3` is valid here because we first checked it against
  // `end`.
  if (it != std::end(v) && *it == 3) {
    // Similarly here, push_back invalidates all the previous iterators.
    v.push_back(4);
  }
  // Invalid because we might have entered the condition block.
  ++it;
}
