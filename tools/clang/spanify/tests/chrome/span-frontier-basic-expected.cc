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
  // Expected rewrite:
  // base::span<int> spanified_2 = buf;
  base::span<int> spanified_2 = buf;
  // Expected rewrite:
  // base::span<int> spanified_1 = spanified_2;
  base::span<int> spanified_1 = spanified_2;  // Expect: frontier not applied.
  // Expected rewrite:
  // int* not_spanified_2 = spanified_2.data();
  int* not_spanified_2 = spanified_2.data();  // Expect: frontier applied
  int* not_spanified_1 = not_spanified_2;     // Expect: frontier not applied.
  spanified_1[0] = 0;
}

// Test the case that the arrow operator is used instead of the dot operator.
// The lhs of the operator is a pointer, so we need to dereference it.
void test_frontier_basic2() {
  std::vector<int> buf(5, 5);
  std::vector<int>* buf_ptr;
  // Expected rewrite:
  // base::span<int> spanified_2 = *buf_ptr;
  base::span<int> spanified_2 = *buf_ptr;
  // Expected rewrite:
  // base::span<int> spanified_1 = spanified_2;
  base::span<int> spanified_1 = spanified_2;
  spanified_1[0] = 0;
}

// Test the case that the arrow operator is used instead of the dot operator.
// The lhs of the operator can be a non-simple expression, e.g. a function call.
void test_frontier_basic3() {
  extern std::vector<int>* func();
  // Expected rewrite:
  // base::span<int> spanified_2 = *func();
  base::span<int> spanified_2 = *func();
  // Expected rewrite:
  // base::span<int> spanified_1 = spanified_2;
  base::span<int> spanified_1 = spanified_2;
  spanified_1[0] = 0;
}
