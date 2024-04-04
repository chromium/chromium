// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
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

void IteratorsMismatched(std::vector<int>& v1, std::vector<int>& v2) {
  auto it = std::find(std::begin(v1), std::end(v1), 3);

  // Invalid because mismatched iterators.
  v2.erase(it);
}
