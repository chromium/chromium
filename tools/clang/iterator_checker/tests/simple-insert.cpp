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
