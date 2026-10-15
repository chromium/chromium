// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `constexpr` on an array contributes its constness through the declaration
// specifiers rather than through the element type, so Clang wraps the written
// `ArrayTypeLoc` of the declaration in a `QualifiedTypeLoc`. A use site that
// asks the bound node for a `clang::ArrayTypeLoc` directly then gets null and
// concludes "this is not a C array", which downgrades the rewrite to
// `std_array.subspan(n)` -- and `std::array` has no `subspan()`.
//
// Each case below is written twice, once `constexpr` and once plain `const`.
// The two spellings denote the same type, so they must produce the same
// rewrite.

#include <array>

#include "base/containers/span.h"

void Sink(const int* p);

// Expected rewrite:
// constexpr std::array<int, 4> kConstexprArray = {0, 1, 2, 3};
constexpr std::array<int, 4> kConstexprArray = {0, 1, 2, 3};

// Expected rewrite:
// const std::array<int, 4> kConstArray = {0, 1, 2, 3};
const std::array<int, 4> kConstArray = {0, 1, 2, 3};

void PointerArithmeticToAnExternalSink() {
  // Expected rewrite:
  // Sink(base::span<const int>(kConstexprArray).subspan(1u).data());
  Sink(base::span<const int>(kConstexprArray).subspan(1u).data());

  // Expected rewrite:
  // Sink(base::span<const int>(kConstArray).subspan(1u).data());
  Sink(base::span<const int>(kConstArray).subspan(1u).data());
}

unsigned UnsafeIndex();

// Expected rewrite:
// int SpanifiedSink(base::span<const int> buffer) {
int SpanifiedSink(base::span<const int> buffer) {
  return buffer[UnsafeIndex()];
}

void PointerArithmeticToASpanifiedSink() {
  // Expected rewrite:
  // SpanifiedSink(base::span<const int>(kConstexprArray).subspan(1u));
  SpanifiedSink(base::span<const int>(kConstexprArray).subspan(1u));

  // Expected rewrite:
  // SpanifiedSink(base::span<const int>(kConstArray).subspan(1u));
  SpanifiedSink(base::span<const int>(kConstArray).subspan(1u));
}

void AddressOfElementToASpanifiedSink() {
  // Expected rewrite:
  // SpanifiedSink(base::span<const int>(kConstexprArray).subspan(1u));
  SpanifiedSink(base::span<const int>(kConstexprArray).subspan(1u));

  // Expected rewrite:
  // SpanifiedSink(base::span<const int>(kConstArray).subspan(1u));
  SpanifiedSink(base::span<const int>(kConstArray).subspan(1u));
}

#define UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]

UNSAFE_BUFFER_USAGE void UnsafeSink(const int* p);

void AddressOfElementToAnUnsafeBufferUsageSink() {
  // No rewrite expected: `&arr[1]` is also valid on a `std::array`.
  UnsafeSink(&kConstexprArray[1]);

  // No rewrite expected: `&arr[1]` is also valid on a `std::array`.
  UnsafeSink(&kConstArray[1]);
}
