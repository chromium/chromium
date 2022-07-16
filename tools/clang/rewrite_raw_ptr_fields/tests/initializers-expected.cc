// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

class SomeClass;

SomeClass* GetPointer();

class MyClass {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field = GetPointer();
  raw_ptr<SomeClass> raw_ptr_field = GetPointer();
};
