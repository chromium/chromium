// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

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
  int* spanified_2 = buf.data();
  int* spanified_1 = spanified_2;          // Expect: frontier not applied.
  int* not_spanified_2 = spanified_2;      // Expect: frontier applied
  int* not_spanified_1 = not_spanified_2;  // Expect: frontier not applied.
  spanified_1[0] = 0;
}
