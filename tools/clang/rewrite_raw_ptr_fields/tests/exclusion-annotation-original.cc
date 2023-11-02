// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

class SomeClass;

class MyClass {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
};

struct MyStruct {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
};

template <typename T>
class MyTemplate {
  // Expected rewrite: raw_ptr<T> raw_ptr_field;
  T* raw_ptr_field;
  RAW_PTR_EXCLUSION T* excluded_raw_ptr_field;
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
  typename MaybeProvidesType<T>::Type* raw_ptr_field;
  RAW_PTR_EXCLUSION
  typename MaybeProvidesType<T>::Type* excluded_raw_ptr_field;
};
