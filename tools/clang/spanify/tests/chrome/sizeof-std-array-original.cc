// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstddef>
#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

// `sizeof(c_array)` is kept as is when the array becomes a `std::array`, whose
// size is the same. Unlike `std_array.size()`, `sizeof` doesn't evaluate its
// operand, so it keeps compiling wherever it did.

struct Holder {
  // Expected rewrite:
  // std::array<int, 4> arr;
  int arr[4];
};

// Through a pointer, `holder->arr.size()` isn't a constant expression. Through
// a reference, `holder_ref.arr.size()` isn't one before C++23.
void ThroughPointerAndReference(const Holder* holder,
                                const Holder& holder_ref) {
  std::ignore = holder->arr[UnsafeIndex()];
  // No rewrite expected.
  char copy[sizeof(holder->arr)];
  std::ignore = copy;
  // No rewrite expected.
  std::array<char, sizeof holder->arr> template_argument;
  std::ignore = template_argument;
  // No rewrite expected.
  static_assert(sizeof(holder_ref.arr) == 4 * sizeof(int));
}

// `GetHolder()` isn't called by `sizeof`.
struct OtherHolder {
  // Expected rewrite:
  // std::array<int, 4> arr;
  int arr[4];
};
OtherHolder& GetHolder();
void SideEffects() {
  std::ignore = GetHolder().arr[UnsafeIndex()];
  // No rewrite expected.
  std::ignore = sizeof(GetHolder().arr);
}

template <size_t N>
void TemplateSize() {
  // Expected rewrite:
  // std::array<int, N> buf = {};
  int buf[N] = {};
  std::ignore = buf[UnsafeIndex()];
  // No rewrite expected.
  std::ignore = sizeof(buf);
}
template void TemplateSize<4>();

// `sizeof` of a pointer is the size of the pointer, not the size of the span
// it would become. The pointer is not rewritten.
//
// No rewrite expected.
void Pointer(int* ptr) {
  std::ignore = ptr[UnsafeIndex()];
  std::ignore = sizeof(ptr);
}

// Same when the `sizeof` is spelled in a macro body.
//
// No rewrite expected.
#define BYTES(x) sizeof(x)
void MacroPointer(int* ptr) {
  std::ignore = ptr[UnsafeIndex()];
  std::ignore = BYTES(ptr);
}

// `sizeof(buf + 1)` is the size of a pointer: `buf` is not rewritten.
//
// No rewrite expected.
void PointerArithmetic() {
  int buf[4] = {};
  std::ignore = buf[UnsafeIndex()];
  std::ignore = sizeof(buf + 1);
}

// The operand of `sizeof` is a type here. `ptr` is used in its size. Not valid
// Chromium code (VLA): this only checks that the tool doesn't crash.
//
// No rewrite expected.
void VariableLengthArrayType(char* ptr) {
  std::ignore = ptr[UnsafeIndex()];
  std::ignore = sizeof(char[(size_t)ptr]);
}
