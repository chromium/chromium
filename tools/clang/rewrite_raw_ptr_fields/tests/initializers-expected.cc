// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

class SomeClass;

SomeClass* GetPointer();

class MyClass {
  // Expected rewrite: CheckedPtr<SomeClass> raw_ptr_field = GetPointer();
  CheckedPtr<SomeClass> raw_ptr_field = GetPointer();
};
