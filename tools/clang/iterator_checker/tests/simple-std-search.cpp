// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void StdSearchValidIterator(const std::vector<int>& v, std::vector<int>& p) {
  auto it = std::search(std::begin(v), std::end(v), std::begin(p), std::end(p));
  // TODO: The error `Potentially invalid iterator comparison` shouldn't be
  // emitted here.
  if (it != std::end(v)) {
    *it;  // Valid.
  }
}

void StdSearchInvalidIterator(const std::vector<int>& v, std::vector<int>& p) {
  auto it = std::search(std::begin(v), std::end(v), std::begin(p), std::end(p));
  *it;  // Invalid access.
}
