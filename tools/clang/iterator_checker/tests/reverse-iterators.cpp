// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

void ReverseIteratorInvalid(std::vector<int>& v) {
  auto it = std::rbegin(v);
  // TODO(329133423): Support reverse iterators.
  *it = 10;  // Iterator potentially invalid.
}

void ReverseIteratorValid(std::vector<int>& v) {
  auto it = std::rbegin(v);
  if (it == std::rend(v)) {
    return;
  }
  *it = 10;
}