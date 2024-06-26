// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

#define __container [[clang::annotate_type("container")]]

auto __container GetBegin(const std::vector<int>& __container v) {
  return v.begin();
}

// This function uses the new defined `GetBegin` function, which doesn't have
// any hardcoded annotation.
void UseAnnotatedFunction(std::vector<int>& v) {
  auto it = GetBegin(v);

  // Invalid because it was not checked against `end`.
  *it;

  // Fine comparison because we are using the returned iterator.
  if (it != std::end(v)) {
    // Valid because it was checked against `end`.
    *it;

    // This invalidates the container.
    v.clear();

    // Invalid because it was invalidated by `v.clear()`.
    *it;
  }
}
