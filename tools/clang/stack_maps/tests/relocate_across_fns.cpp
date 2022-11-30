// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests whether objects in Handles which are passed in function arguments
// can be re-accessed after a moving collection.
//
// A GC call ensures that every Handle'd object is relocated. Attempting to
// deref the value after this call should yield the same object that was
// initially allocated.

#include <assert.h>
#include "objects.h"
#include "tests.h"

extern Handle<HeapObject> AllocateHeapObject(long data);

long expected = 1234;

__attribute__((noinline)) Handle<HeapObject> bar(Handle<HeapObject> x) {
  GC();
  assert((*x).data == expected && "GC Objects differ across a collection");
  return x;
}

__attribute__((noinline)) Handle<HeapObject> foo(Handle<HeapObject> x) {
  assert((*x).data == expected && "GC Objects differ across a collection");
  return bar(x);
}

__attribute__((noinline)) Handle<HeapObject> baz(Handle<HeapObject> x) {
  assert((*x).data == expected && "GC Objects differ across a collection");
  return foo(x);
}

__attribute__((noinline)) void test_relocation() {
  auto handle = AllocateHeapObject(expected);
  baz(handle);
  assert((*handle).data == expected && "GC Objects differ across a collection");
}

int main() {
  InitGC();
  test_relocation();
  TeardownGC();
  return 0;
}
