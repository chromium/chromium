// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

void InvalidatesAllIterators() {
  std::vector<int> v = {1, 2, 3};
  auto it1 = v.begin();
  auto it2 = v.begin() + 1;
  auto it3 = v.begin() + 2;

  if (it1 == v.end() || it2 == v.end() || it3 == v.end()) {
    return;
  }

  // Invalidates all the iterators
  v.clear();

  // All these are invalid
  *it1 = 1;
  *it2 = 2;
  *it3 = 3;
}

void IteratorsStayValid() {
  std::vector<int> v = {1, 2, 3};
  auto it1 = v.begin();
  auto it2 = v.begin() + 1;
  auto it3 = v.begin() + 2;

  if (it1 == v.end() || it2 == v.end() || it3 == v.end()) {
    return;
  }

  // All these are valid because we checked them
  *it1 = 1;
  *it2 = 2;
  *it3 = 3;
}
