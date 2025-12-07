// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

unsigned UnsafeIndex();

// Expected rewrite:
// void UseBufferUnsafely(base::span<int> buffer) {
void UseBufferUnsafely(base::span<int> buffer) {
  buffer[UnsafeIndex()] = 13;
}

void Func() {
  // No expected rewrite: nothing unsafe happens in this scope.
  int array[10];

  // Even if the underlying C-style array isn't rewritten, the array is
  // passed to a spanified function - this requires that we rewrite the
  // call expression.
  //
  // Expected rewrite:
  // UseBufferUnsafely(array);
  UseBufferUnsafely(array);

  // With an offset (subspan) in play, we need to also wrap the array
  // declref in a `base::span()`.
  // Expected rewrite:
  // UseBufferUnsafely(base::span<int>(array).subspan(1u));
  UseBufferUnsafely(base::span<int>(array).subspan(1u));
}

class HasFrontier {
 public:
  // Expected rewrite:
  // void CopyFrom(base::span<const int> from) {
  void CopyFrom(base::span<const int> from) {
    // Expected rewrite:
    // std::copy(from.data(), from.subspan(4u).data(), ints_);
    std::copy(from.data(), from.subspan(4u).data(), ints_);
  }

  // Expected rewrite:
  // void CopyFromMiraclePtr(base::raw_span<const int, AllowPtrArithmetic> from)
  void CopyFromMiraclePtr(base::raw_span<const int, AllowPtrArithmetic> from) {
    // Expected rewrite:
    // std::copy(from.data(), from.subspan(4u).data(), ints_);
    std::copy(from.data(), from.subspan(4u).data(), ints_);
  }

 private:
  int ints_[4];
};

void TriggerSpanificationOfHasFrontier() {
  static constexpr int from[4] = {0, 1, 2, 3};
  HasFrontier foo = HasFrontier();
  foo.CopyFrom(from);

  // Inappropriate use of `raw_ptr` for testing only.
  // Expected rewrite:
  // base::raw_span<const int, AllowPtrArithmetic> miracleptr_from(from);
  base::raw_span<const int, AllowPtrArithmetic> miracleptr_from(from);
  foo.CopyFromMiraclePtr(miracleptr_from);
}
