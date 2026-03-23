// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test ensures the rewriter differentiates between namespaces that
// share names but reside in different branches of the namespace tree.
//
// Hierarchy & Expected Resolution:
// (Global Scope)
// └── partition_alloc (ns)
//     │
//     ├── internal::another_ns (ns) [Nested]
//     │   ├── base (ns)
//     │   │   └── unused_function()
//     │   └── (anonymous ns)
//     │       └── level_1()   <----- 'internal::base::span'
//     │
//     ├── another_ns (ns)
//     │   ├── internal (ns)
//     │   │   └── unused_function()
//     │   ├── level_2()   <--------- 'partition_alloc::internal::base::span'
//     │   └── (anonymous ns)
//     │       └── level_3()   <----- 'partition_alloc::internal::base::span'
//     │
//     └── (anonymous ns)
//         ├── partition_alloc (ns)
//         │   └── level_4()   <----- '::partition_alloc::internal::base::span'
//         └── another_ns (ns)
//             └── level_5()   <----- '::partition_alloc::internal::base::span'

namespace partition_alloc {

namespace internal::another_ns {

namespace base {

void unused_function() {
  int var = 2;
}

}  // namespace base

namespace {

void level_1() {
  int buf[10];
  // Unexpected rewrite (should start with `internal` ns)
  // base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace

}  // namespace internal::another_ns

namespace another_ns {

namespace internal {

void unused_function() {
  int var = 2;
}

}  // namespace internal

void level_2() {
  int buf[10];
  // Unexpected rewrite (should start with `partition_alloc` ns)
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

namespace {

void level_3() {
  int buf[10];
  // Unexpected rewrite (should start with `partition_alloc` ns)
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace

}  //  namespace another_ns

namespace {

namespace partition_alloc {

void level_4() {
  int buf[10];
  // Unexpected rewrite (should add `::partition_alloc` at the beginning)
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace partition_alloc

namespace another_ns {

void level_5() {
  int buf[10];
  // Unexpected rewrite (should add `::partition_alloc` at the beginning)
  // internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace another_ns

}  // namespace

}  //  namespace partition_alloc
