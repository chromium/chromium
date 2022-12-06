// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class SomeClass;

// Expected rewrite: typedef raw_ptr<SomeClass> SomeClassPtrTypedef.
// TODO(lukasza): Handle rewriting typedefs.
typedef SomeClass* SomeClassPtrTypedef;

// Expected rewrite: using SomeClassPtrTypeAlias = raw_ptr<SomeClass>;
// TODO(lukasza): Handle rewriting type aliases.
using SomeClassPtrTypeAlias = SomeClass*;

// No rewrite.
typedef SomeClass& SomeClassRefTypedef;

// No rewrite.
using SomeClassRefTypeAlias = SomeClass&;

struct MyStruct {
  // No rewrite expected here.
  SomeClassPtrTypedef field1;
  SomeClassPtrTypeAlias field2;

  // Only "shallow" rewrite expected here (without unsugaring/inlining the type
  // aliases).  So:
  // Expected rewrite: raw_ptr<SomeClassPtrTypedef> field3;
  raw_ptr<SomeClassPtrTypedef> field3;
  // Expected rewrite: raw_ptr<SomeClassPtrTypeAlias> field4;
  raw_ptr<SomeClassPtrTypeAlias> field4;

  // No rewrite expected here.
  SomeClassRefTypedef ref_field1;
  SomeClassRefTypeAlias ref_field2;

  // Expected rewrite: const raw_ref<SomeClass> ref_field3;
  const raw_ref<SomeClass> ref_field3;
  // Expected rewrite: const raw_ref<SomeClass> ref_field4;
  const raw_ref<SomeClass> ref_field4;
};
