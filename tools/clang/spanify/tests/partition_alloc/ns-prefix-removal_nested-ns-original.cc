// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test verifies that the rewriter correctly resolves the path to
// `partition_alloc::internal::base::span` from various nesting depths.
// As we traverse "down" the primary namespace path, the required
// qualification should become progressively shorter:

// Hierarchy & Expected Resolution:
// (Global Scope)
// └── level_0()   <------------------- 'partition_alloc::internal::base::span'
//     └── (partition_alloc) [ns]
//         ├── level_1()   <--------------- 'internal::base::span'
//         ├── (another_ns) [ns]
//         │   └── level_1_1()   <--------- 'internal::base::span'
//         └── (internal) [ns]
//             ├── level_2()   <----------- 'base::span'
//             ├── (another_ns) [ns]
//             │   └── level_2_1()   <----- 'base::span
//             └── (base) [ns]
//                 ├── level_3()   <------- 'span'
//                 └── (another_ns) [ns]
//                     └── level_3_1()   <- 'span'

void level_0() {
  // This function doesn't share any namespace with target.
  // It is expected to use the full namespace path.
  int buf[10];
  // Expected rewrite:
  // partition_alloc::internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

namespace partition_alloc {

void level_1() {
  // Already in `partition_alloc`.
  int buf[10];
  // Expected rewrite:
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

namespace another_ns {

void level_1_1() {
  int buf[10];
  // Expected rewrite:
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace another_ns

namespace internal {

void level_2() {
  int buf[10];
  // Expected rewrite:
  // base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

namespace another_ns {

void level_2_1() {
  int buf[10];
  // Expected rewrite:
  // base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace another_ns

namespace base {

void level_3() {
  int buf[10];
  // Expected rewrite:
  // span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

namespace another_ns {

void level_3_1() {
  int buf[10];
  // Expected rewrite:
  // span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace another_ns
}  // namespace base
}  // namespace internal
}  // namespace partition_alloc
