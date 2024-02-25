// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

// Checks that comparison between valid iterators but from different containers
// triggers an error.
void ValidIteratorsWithDifferentContainers() {
  std::vector<int> v1, v2;

  auto it1 = v1.begin();
  auto it2 = v2.begin();

  if (it1 == v1.end() || it2 == v2.end()) {
    return;
  }

  // Iterators are now valid, because we checked them against the `end`
  // iterator.
  *it1;
  *it2;

  // Wrong comparison, those iterators are not coming from the same container.
  if (it1 == it2) {
    return;
  }
}

// Checks that iterators checked against the wrong `end` iterator triggers an
// error.
void WrongEndIteratorCheck() {
  std::vector<int> v1, v2;

  // Wrong comparison, iterators are not from the same containers.
  if (v1.begin() != v2.end()) {
    return;
  }

  auto it = v1.begin();
  if (it != v2.end()) {
    return;
  }
}
