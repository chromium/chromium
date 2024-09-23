// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

class SomeClass {
 public:
  int member;
};

class MyClass {
 public:
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;

  // Expected rewrite: raw_ref<SomeClass> raw_ref_field;
  SomeClass& raw_ref_field;

  // Expected rewrite: base::raw_span<SomeClass> span_field;
  base::span<SomeClass> span_field;

  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass& excluded_raw_ref_field;
  RAW_PTR_EXCLUSION base::span<SomeClass> excluded_span_field;
};

struct MyStruct {
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  SomeClass* raw_ptr_field;
  // Expected rewrite: raw_ref<SomeClass> raw_ref_field;
  SomeClass& raw_ref_field;
  // Expected rewrite: base::raw_span<SomeClass> span_field;
  base::span<SomeClass> span_field;
  RAW_PTR_EXCLUSION SomeClass* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION SomeClass& excluded_raw_ref_field;
  RAW_PTR_EXCLUSION base::span<SomeClass> excluded_span_field;
};

template <typename T>
class MyTemplate {
 public:
  // Expected rewrite: raw_ptr<T> raw_ptr_field;
  T* raw_ptr_field;

  // Expected rewrite: raw_ref<T> raw_ref_field;
  T& raw_ref_field;

  // Expected rewrite: base::raw_span<T> span_field;
  base::span<T> span_field;

  RAW_PTR_EXCLUSION T* excluded_raw_ptr_field;
  RAW_PTR_EXCLUSION T& excluded_raw_ref_field;
  RAW_PTR_EXCLUSION base::span<T> excluded_span_field;
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

  // Expected rewrite:
  // base::raw_span<typename MaybeProvidesType<T>::Type> span_field;
  base::span<typename MaybeProvidesType<T>::Type> span_field;
  RAW_PTR_EXCLUSION
  base::span<typename MaybeProvidesType<T>::Type> excluded_span_field;
};
