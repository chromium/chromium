// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/containers/span.h"

class SomeClass;

// Based on Chromium's //base/thread_annotations.h
#define GUARDED_BY(lock) __attribute__((guarded_by(lock)))

class MyClass {
  MyClass(SomeClass& s) : ref_field(s), lock(0) {}
  // Expected rewrite: raw_ptr<SomeClass> ptr_field GUARDED_BY(lock);
  SomeClass* ptr_field GUARDED_BY(lock);
  // Expected rewrite: const raw_ref<SomeClass> ref_field GUARDED_BY(lock);
  SomeClass& ref_field GUARDED_BY(lock);
  // Expected rewrite: base::raw_span<SomeClass> span_field GUARDED_BY(lock);
  base::span<SomeClass> span_field GUARDED_BY(lock);
  // Expected rewrite: std::vector<base::raw_span<SomeClass>>
  // container_of_span_field GUARDED_BY(lock);
  std::vector<base::span<SomeClass>> container_of_span_field GUARDED_BY(lock);
  int lock;
};
