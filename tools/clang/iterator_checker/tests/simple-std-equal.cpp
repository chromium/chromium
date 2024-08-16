// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

bool StdEqual(const std::vector<int>& v, const std::vector<int>& p) {
  return std::equal(v.begin(), v.end(), std::begin(p), std::end(p));
}

bool StdEqualMismatched(const std::vector<int>& v, const std::vector<int>& p) {
  // This is not fine because using mismatched iterators.
  return std::equal(std::begin(v), std::begin(p), p.begin(), v.end());
}
