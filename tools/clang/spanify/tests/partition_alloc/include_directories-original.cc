// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

void requires_span_h() {
  int buf[10];
  // Expected rewrite:
  // partition_alloc::internal::base::span<int> ptr = buf;
  int* ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

int* requires_safe_conversions() {
  static int base_numbers[] = {1, 2, 3, 4};

  bool condition = true;
  // Expected rewrite:
  // partition_alloc::internal::base::span<int> var1 = base_numbers;
  int* var1 = base_numbers;
  int offset = 1;
  // Expected rewrite:
  // return var1.subspan(base::checked_cast<size_t>(offset)).data();
  return var1 + offset;
}

struct S2 {
  // Expected rewrite:
  // S2(partition_alloc::internal::base::span<int> ptr) : ptr_(ptr) {}
  S2(int* ptr) : ptr_(ptr) {}

  int* get_and_advance() {
    // Unexpected rewrite (parenthesis not needed surrounding PostIncrementSpan)
    // return (base::PostIncrementSpan(ptr_)).data();
    return ptr_++;
  }

  // Expected rewrite:
  // partition_alloc::internal::base::span<int> ptr_;
  int* ptr_;
};
