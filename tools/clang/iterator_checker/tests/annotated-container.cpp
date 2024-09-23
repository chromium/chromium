// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <algorithm>
#include <vector>

#define __container [[clang::annotate("container")]]

template <typename T>
class vector : public std::vector<T> {
 public:
  using iterator __container = typename std::vector<T>::iterator;

  iterator begin() { return std::vector<T>::begin(); }
};

// This function uses the new defined `vector` container, which doesn't have any
// hardcoded annotation.
void UseAnnotatedContainer(vector<int>& v) {
  auto it = v.begin();

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
