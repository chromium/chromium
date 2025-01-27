// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/span.h"

// Test the frontier change are applied correctly. Below, there are 3 kinds of
// frontiers, but only 1 of them is spanified.
//        ┌──────────────────┐
//        │spanified_2       │
//        └▲────────────────▲┘
// ┌───────┴───────┐┌───────┴───────┐
// │not_spanified_2││spanified_1 (*)│ (* = buffer usage)
// └▲──────────────┘└───────────────┘
// ┌┴──────────────┐
// │not_spanified_1│
// └───────────────┘
void test_frontier_basic() {
  std::vector<int> buf(5, 5);
  base::span<int> spanified_2 = buf;
  base::span<int> spanified_1 = spanified_2;  // Expect: frontier not applied.
  int* not_spanified_2 = spanified_2.data();  // Expect: frontier applied
  int* not_spanified_1 = not_spanified_2;     // Expect: frontier not applied.
  spanified_1[0] = 0;
}
