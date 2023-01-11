// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

class SomeClass {
 public:
  int member;
};

class MyClass {
 public:
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  raw_ptr<SomeClass> raw_ptr_field;

  // Expected rewrite: raw_ref<SomeClass> raw_ref_field;
  const raw_ref<SomeClass> raw_ref_field;

  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass& excluded_raw_ref_field;
};

struct MyStruct {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  raw_ptr<SomeClass> raw_ptr_field;
  // Expected rewrite: raw_ref<SomeClass> raw_ref_field;
  const raw_ref<SomeClass> raw_ref_field;
  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass& excluded_raw_ref_field;
};

template <typename T>
class MyTemplate {
 public:
  // Expected rewrite: raw_ptr<T> raw_ptr_field;
  raw_ptr<T> raw_ptr_field;

  // Expected rewrite: raw_ref<T> raw_ref_field;
  const raw_ref<T> raw_ref_field;

  RAW_PTR_EXCLUSION T* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION T& excluded_raw_ref_field;
};

// The field below won't compile without the |typename| keyword (because
// at this point we don't know if MaybeProvidesType<T>::Type is a type,
// value or something else).  Let's see if the rewriter will preserve
// preserve the |typename| keyword.
template <typename T>
struct MaybeProvidesType;
template <typename T>
struct DependentNameTest {
  // Expected rewrite: raw_ptr<typename MaybeProvidesType<T>::Type> field;
  raw_ptr<typename MaybeProvidesType<T>::Type> raw_ptr_field;
  RAW_PTR_EXCLUSION
  typename MaybeProvidesType<T>::Type* excluded_raw_ptr_field;
};
