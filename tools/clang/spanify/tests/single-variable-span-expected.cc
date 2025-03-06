// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

// Tests related to single_element_expr matcher.

#include "base/containers/span.h"

void processIntBuffer(base::span<int> buf) {
  std::ignore = buf[0];
}

void testPointerPassing() {
  int singleInt;
  // Expected rewrite:
  // processIntBuffer(base::span<int, 1>(&singleInt))
  processIntBuffer(base::span<int, 1>(&singleInt));

  int intArray[10];
  // Not using &.
  // No rewrite expected.
  processIntBuffer(intArray);
  // Operand for & is not a simple variable.
  // No rewrite expected. (crrev.com/c/6286045)
  processIntBuffer(&intArray[0]);

  std::vector<int> intVector;
  // We know how to get size from Vector so just leave it alone to
  // construct a span.
  // Expected rewrite:
  // processIntBuffer(intVector);
  processIntBuffer(intVector);
}

// Function that takes a pointer to an integer pointer.
void processIntPointerBuffer(base::span<int*> pointerToData) {
  std::ignore = pointerToData[0];
}

void testPointerToPointerPassing() {
  int* singleIntPointer;
  // Expected rewrite:
  // processIntPointerBuffer(base::span<int*, 1>(&singleIntPointer));
  processIntPointerBuffer(base::span<int*, 1>(&singleIntPointer));

  int* intArrayOfPointers[10];
  // Not using &.
  // No rewrite expected.
  processIntPointerBuffer(intArrayOfPointers);
  // Operand for & is not a simple variable.
  // No rewrite expected. (crrev.com/c/6286045)
  processIntPointerBuffer(&intArrayOfPointers[0]);

  std::vector<int> intVector;
  // Operand for & is not a simple variable.
  // No rewrite expected. (crrev.com/c/6286045)
  processIntPointerBuffer(&intArrayOfPointers[0]);
}

struct MyStruct {
  int field;
};

void testFieldPointerPassing() {
  MyStruct myStruct;
  // Expected rewrite:
  // processIntBuffer(base::span<int, 1>(&myStruct.field));
  processIntBuffer(base::span<int, 1>(&myStruct.field));
}

void testParamPointerPassing(int param) {
  // Expected rewrite:
  // processIntBuffer(base::span<int, 1>(&param));
  processIntBuffer(base::span<int, 1>(&param));
}
