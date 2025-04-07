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
  // processIntBuffer(base::span<int, 1>(&singleInt, 1u))
  processIntBuffer(base::span<int, 1>(&singleInt, 1u));

  int intArray[10];
  // Not using &.
  // No rewrite expected.
  processIntBuffer(intArray);
  // Triggers pointer-into-array rewriting (issue 402806166).
  // Expected rewrite:
  // processIntBuffer(intArray);
  processIntBuffer(intArray);

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
  // processIntPointerBuffer(base::span<int*, 1>(&singleIntPointer, 1u));
  processIntPointerBuffer(base::span<int*, 1>(&singleIntPointer, 1u));

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
  // processIntBuffer(base::span<int, 1>(&myStruct.field, 1u));
  processIntBuffer(base::span<int, 1>(&myStruct.field, 1u));
}

void testParamPointerPassing(int param) {
  // Expected rewrite:
  // processIntBuffer(base::span<int, 1>(&param, 1u));
  processIntBuffer(base::span<int, 1>(&param, 1u));
}
