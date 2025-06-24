// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <tuple>
#include <vector>

#include "base/containers/span.h"

// Expected rewrite:
// constexpr base::span<int> getPtr() {
constexpr base::span<int> getPtr() {
  return {};
}

constexpr int default_data[] = {1, 2};

struct A {
  static const int default_value = 42;

  // Expected rewrite:
  // base::span<int> buffer1 = {};
  base::span<int> buffer1 = {};
  // No rewrite expected. Added to document current behavior.
  int* buffer2 = NULL;
  // No rewrite expected. Added to document current behavior.
  int* buffer3 = 0;
  // Expected rewrite:
  // base::span<int> buffer4 = getPtr();
  base::span<int> buffer4 = getPtr();
  // Expected rewrite:
  // base::span<const int> buffer5 = base::span_from_ref(default_value);
  base::span<const int> buffer5 = base::span_from_ref(default_value);
  // Expected rewrite:
  // base::span<const int> buffer6 = default_data;
  base::span<const int> buffer6 = default_data;
};

void fct() {
  A a;
  // Trigger rewrites.
  std::ignore = a.buffer1[0];
  std::ignore = a.buffer2[0];
  std::ignore = a.buffer3[0];
  std::ignore = a.buffer4[0];
  std::ignore = a.buffer5[0];
  std::ignore = a.buffer6[0];
}
