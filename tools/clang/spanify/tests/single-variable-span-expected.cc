// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <vector>

// Tests related to single_element_expr matcher.

#include "base/containers/span.h"

void processIntBuffer(base::span<int> buf) {
  std::ignore = buf[0];
}

void processUint8Buffer(base::span<uint8_t> buf) {
  std::ignore = buf[0];
}

void testPointerPassing() {
  int singleInt;
  // Expected rewrite:
  // processIntBuffer(base::span_from_ref(singleInt));
  processIntBuffer(base::span_from_ref(singleInt));
  // processUint8Buffer(base::as_writable_byte_span(
  //    base::span_from_ref(singleInt)));
  processUint8Buffer(
      base::as_writable_byte_span(base::span_from_ref(singleInt)));

  int intArray[10];
  // Not using &.
  // No rewrite expected.
  processIntBuffer(intArray);
  // Triggers pointer-into-array rewriting (issue 402806166).
  // Expected rewrite:
  // processIntBuffer(intArray);
  processIntBuffer(intArray);
  // Do not rewrite because code may be expecting an buffer with >1 size.
  // No rewrite expected.
  processUint8Buffer(reinterpret_cast<uint8_t*>(&intArray));

  std::array<int, 5> stdArray;
  // Do not rewrite because code may be expecting an buffer with >1 size.
  // No rewrite expected.
  processUint8Buffer(reinterpret_cast<uint8_t*>(&stdArray));

  std::vector<int> intVector;
  // We know how to get size from Vector so just leave it alone to
  // construct a span.
  // Expected rewrite:
  // processIntBuffer(intVector);
  processIntBuffer(intVector);

  void* voidPtr;
  // void** should get rewritten.
  // Expected rewrite:
  // processUint8Buffer(
  //   base::as_writable_byte_span(base::span_from_ref(voidPtr)));
  processUint8Buffer(base::as_writable_byte_span(base::span_from_ref(voidPtr)));
}

// Function that takes a pointer to an integer pointer.
void processIntPointerBuffer(base::span<int*> pointerToData) {
  std::ignore = pointerToData[0];
}

void testPointerToPointerPassing() {
  int* singleIntPointer;
  // Expected rewrite:
  // processIntPointerBuffer(base::span_from_ref(singleIntPointer));
  processIntPointerBuffer(base::span_from_ref(singleIntPointer));

  int* intArrayOfPointers[10];
  // Not using &.
  // No rewrite expected.
  processIntPointerBuffer(intArrayOfPointers);
  // Triggers pointer-into-array rewriting (issue 402806166).
  // Expected rewrite:
  // processIntPointerBuffer(intArrayOfPointers);
  processIntPointerBuffer(intArrayOfPointers);

  std::vector<int*> intVector;
  // Triggers pointer-into-container rewriting (crrev.com/c/6304404).
  // Expected rewrite:
  // processIntPointerBuffer(intVector);
  processIntPointerBuffer(intVector);
}

struct MyStruct {
  int field;
};

void testFieldPointerPassing() {
  MyStruct myStruct;
  // Expected rewrite:
  // processIntBuffer(base::span_from_ref(myStruct.field));
  processIntBuffer(base::span_from_ref(myStruct.field));
}

void testParamPointerPassing(int param) {
  // Expected rewrite:
  // processIntBuffer(base::span_from_ref(param));
  processIntBuffer(base::span_from_ref(param));
}
