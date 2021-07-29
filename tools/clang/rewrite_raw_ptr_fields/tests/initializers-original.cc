// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

SomeClass* GetPointer();

class MyClass {
  // Expected rewrite: CheckedPtr<SomeClass> raw_ptr_field = GetPointer();
  SomeClass* raw_ptr_field = GetPointer();
};
