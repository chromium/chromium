// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void StdSearchValidIterator(const std::vector<int>& v, std::vector<int>& p) {
  auto it = std::search(std::begin(v), std::end(v), std::begin(p), std::end(p));
  if (it != std::end(v)) {
    *it;  // Valid.
  }
}

void StdSearchInvalidIterator(const std::vector<int>& v, std::vector<int>& p) {
  auto it = std::search(std::begin(v), std::end(v), std::begin(p), std::end(p));
  *it;  // Invalid access.
}

void StdSearchMismatchedIterators(std::vector<int>& v1, std::vector<int>& v2) {
  // This is not fine because using mismatched iterators.
  std::search(std::begin(v1), std::end(v2), std::begin(v2), std::end(v1));
}
