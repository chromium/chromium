// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void PassBool(bool value);

void ComparisonCheckAfterSwapContainers(std::vector<int> v1,
                                        std::vector<int> v2) {
  auto it1 = std::begin(v1);
  auto it2 = std::begin(v2);
  if (it1 == std::end(v1) || it2 == std::end(v2)) {
    return;
  }
  // Both iterators are valid before the swap.
  *it1 = 0;
  *it2 = 0;

  v1.swap(v2);

  // After the swap, they are still valid but their containers are swapped.
  *it1 = 0;
  *it2 = 0;

  // This is not valid, because containers are swapped.
  PassBool(it1 != v1.end());
  PassBool(it2 != v2.end());

  // This is valid, because containers are swapped.
  PassBool(it1 != v2.end());
  PassBool(it2 != v1.end());
}
