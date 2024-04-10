// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

void SameSourceError(const std::vector<std::vector<int>>& vector_of_vector,
                     const std::vector<int>& v) {
  auto it = std::begin(vector_of_vector);

  if (it->begin() == v.begin()) {
    return;
  }
}
