// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

// Based on Chromium's //base/thread_annotations.h
#define GUARDED_BY(lock) __attribute__((guarded_by(lock)))

class MyClass {
  // Expected rewrite: CheckedPtr<SomeClass> field GUARDED_BY(lock);
  SomeClass* field GUARDED_BY(lock);
  int lock;
};
