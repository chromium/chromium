// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests whether on-stack roots inside a single function are updated
// across a collection.
//
// A GC call ensures that every Handle'd object is relocated. Attempting to
// deref the value after this call should yield the same object that was
// initially allocated.

#include <assert.h>
#include "objects.h"
#include "tests.h"

extern Handle<HeapObject> AllocateHeapObject(long data);

// We noinline for demonstration purposes only - we want to be able to know the
// call graph deterministically to ensure things behave as expected. Inlining
// small functions like this could make debugging more difficult.
__attribute__((noinline)) void test_relocation() {
  auto expected = 1234;
  auto handle = AllocateHeapObject(expected);

  // Relocates all objects in the heap from fromspace to tospace and walks the
  // stack, updating roots to point to the relocated object.
  GC();

  assert((*handle).data == expected && "GC Objects differ across a collection");
}

int main() {
  InitGC();

  test_relocation();

  TeardownGC();
  return 0;
}
