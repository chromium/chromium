// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

int UnsafeIndex();
unsigned int UnsafeUnsignedIndex();

// Expected rewrite:
// void UnsafelyModifyIntSlice(base::span<int> slice) {
void UnsafelyModifyIntSlice(base::span<int> slice) {
  slice[UnsafeIndex()] = 0;
}

void SubspanIntoCStyleArrayIsUnsigned() {
  // Expected rewrite:
  // static auto ints = std::to_array<int>({0, 1, 2});
  static auto ints = std::to_array<int>({0, 1, 2});

  // Unsigned integer literal -> propagate it as written.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(1u));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(1u));

  // Expression that evaluates unsigned -> propagate it as written.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(UnsafeUnsignedIndex()));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(UnsafeUnsignedIndex()));

  // Signed integer literal -> postfix `u`.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u));

  // 1. Different signedness
  // 2. `3` is promoted to unsigned
  // 3. We can see that the expression is unsigned
  // 4. Propagate it as written.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u - 1));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u - 1));

  // Indexing expression is not integer literal and is signed
  // -> wrap the expression in `checked_cast`.
  //
  // Incidentally, this triggers arrayification of `ints`.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(
  //     base::span<int>(ints).subspan(base::checked_cast<size_t>(UnsafeIndex())));
  UnsafelyModifyIntSlice(
      base::span<int>(ints).subspan(base::checked_cast<size_t>(UnsafeIndex())));

  // Compound version of the preceding
  // -> wrap the expression in `checked_cast`.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(
  //     base::span<int>(ints).subspan(base::checked_cast<size_t>(UnsafeIndex()
  //     + 1)));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(
      base::checked_cast<size_t>(UnsafeIndex() + 1)));
}

// The same test cases as `SubspanIntoCStyleArrayIsUnsigned`.
void SubspanIntoContainerishArrayIsUnsigned() {
  static std::vector<int> ints = {0, 1, 2};

  // Unsigned integer literal -> propagate it as written.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(1u));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(1u));

  // Expression that evaluates unsigned -> propagate it as written.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(UnsafeUnsignedIndex()));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(UnsafeUnsignedIndex()));

  // Signed integer literal -> postfix `u`.
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u));

  // 1. Different signedness
  // 2. `3` is promoted to unsigned
  // 3. We can see that the expression is unsigned
  // 4. Propagate it as written.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u - 1));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(2u - 1));

  // Indexing expression is not integer literal and is signed
  // -> wrap the expression in `checked_cast`.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(
  //     base::span<int>(ints).subspan(base::checked_cast<size_t>(UnsafeIndex())));
  UnsafelyModifyIntSlice(
      base::span<int>(ints).subspan(base::checked_cast<size_t>(UnsafeIndex())));

  // Compound version of the preceding
  // -> wrap the expression in `checked_cast`.
  //
  // Expected rewrite:
  // UnsafelyModifyIntSlice(base::span<int>(ints).subspan(
  //     base::checked_cast<size_t>(UnsafeIndex() + 1)));
  UnsafelyModifyIntSlice(base::span<int>(ints).subspan(
      base::checked_cast<size_t>(UnsafeIndex() + 1)));
}
