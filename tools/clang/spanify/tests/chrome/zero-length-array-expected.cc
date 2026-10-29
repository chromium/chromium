// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A zero-length array (a GNU extension) is never rewritten into a
// `std::array<T, 0>`, which isn't equivalent: it isn't empty, so `sizeof` of
// the array and of an enclosing struct change, and every `operator[]` on it
// fails a libc++ hardening check. A trailing zero-length member is usually a
// flexible array indexed past its end. An unsized array (`int data[]`) has no
// size either, and isn't rewritten.

#include <array>
#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

// Unrelated array: still rewritten. Checks that the tool ran at all.
void Unrelated() {
  // Expected rewrite:
  // std::array<int, 4> buf = {};
  std::array<int, 4> buf = {};
  std::ignore = buf[UnsafeIndex()];
}

// No rewrite expected.
struct Header {
  int count;
  int data[0];
};

// No rewrite expected.
void FlexibleArrayMember(Header& header) {
  std::ignore = header.data[UnsafeIndex()];
  std::ignore = sizeof(header.data);
}

// A pointer initialized from a zero-length array gets no size from it, so it
// isn't rewritten either.
//
// No rewrite expected.
void PointerFromZeroLengthArray(Header& header) {
  int* ptr = header.data;
  std::ignore = ptr[UnsafeIndex()];
}

// No rewrite expected.
void Local() {
  int buf[0];
  std::ignore = buf[UnsafeIndex()];
  std::ignore = sizeof(buf);
}

// Any dimension of size 0 counts.
//
// No rewrite expected.
void MultiDimensional() {
  int inner[2][0];
  std::ignore = inner[UnsafeIndex()][UnsafeIndex()];
  std::ignore = sizeof(inner[0]);
}

// A flexible array member has no size.
//
// No rewrite expected.
struct Flex {
  int count;
  int data[];
};
void FlexibleArray(Flex& flex) {
  std::ignore = flex.data[UnsafeIndex()];
}
