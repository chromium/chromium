// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void UseReturnedIteratorAfterInsert(std::vector<int>& v) {
  auto it = std::begin(v);

  it = v.insert(it, 10);

  // Fine comparison because we are using the returned iterator
  if (it != std::end(v)) {
    // Valid because it was checked against `end`.
    *it;
  }
}

void UseInvalidIteratorAfterInsert(std::vector<int>& v) {
  auto it = std::begin(v);

  v.insert(it, 10);

  // Wrong comparison because we are using the invalidated iterator
  if (it != std::end(v)) {
    // Valid because it was checked against `end`.
    *it;
  }
}

void InsertRangeMismatchedIterators(std::vector<int>& v1,
                                    std::vector<int>& v2) {
  auto it = std::begin(v1);

  // This is not fine because using mismatched iterators.
  v1.insert(it, v2.begin(), v1.end());
}

void InsertRange(std::vector<int>& v1, std::vector<int>& v2) {
  auto it = std::begin(v1);

  v1.insert(it, v2.begin(), v2.end());
}
