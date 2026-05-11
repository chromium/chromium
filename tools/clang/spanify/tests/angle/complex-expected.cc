// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "common/span.h"

// Expected rewrite:
// void process(angle::Span<int> p, size_t n) {
void process(angle::Span<int> p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    p[i] = 0;
  }
}

void f() {
  std::vector<int> v(10);
  // Expected rewrite:
  // angle::Span<int> ptr = v;
  angle::Span<int> ptr = v;
  process(ptr, v.size());
  ptr[5] = 1;
}
