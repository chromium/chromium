// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class SomeClass;

SomeClass* GetPointer();
SomeClass& GetReference();

class MyClass {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field = GetPointer();
  raw_ptr<SomeClass> raw_ptr_field = GetPointer();

  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field = GetReference();
  const raw_ref<SomeClass> raw_ref_field = GetReference();
};
