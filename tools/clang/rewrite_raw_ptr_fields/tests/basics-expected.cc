// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class SomeClass;

class MyClass {
  MyClass(SomeClass& s) : raw_ref_field(s) {}
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  raw_ptr<SomeClass> raw_ptr_field;

  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field;
  const raw_ref<SomeClass> raw_ref_field;

  // No rewrite expected.
  int int_field;
};

struct MyStruct {
  MyStruct(SomeClass& s1, SomeClass& s2)
      : raw_ref_field(s1), raw_ref_field2(s2) {}
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  raw_ptr<SomeClass> raw_ptr_field;

  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field;
  const raw_ref<SomeClass> raw_ref_field;

  // No rewrite expected.
  int int_field;

  // "*" next to the field name.  This is non-standard formatting, so
  // "clang-format off" is used to make sure |git cl format| won't change this
  // testcase.
  //
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  // clang-format off
  raw_ptr<SomeClass> raw_ptr_field2;
  // clang-format on

  // "&" next to the field name.  This is non-standard formatting, so
  // "clang-format off" is used to make sure |git cl format| won't change this
  // testcase.
  //
  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field;
  // clang-format off
  const raw_ref<SomeClass> raw_ref_field2;
  // clang-format on
};

template <typename T>
class MyTemplate {
  MyTemplate(T& t) : raw_ref_field(t) {}
  // Expected rewrite: raw_ptr<T> raw_ptr_field;
  raw_ptr<T> raw_ptr_field;

  // Expected rewrite: const raw_ref<T> raw_ref_field;
  const raw_ref<T> raw_ref_field;

  // No rewrite expected.
  int int_field;
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
  raw_ptr<typename MaybeProvidesType<T>::Type> field;

  // Expected rewrite: const raw_ref<typename MaybeProvidesType<T>::Type>
  // field2;
  const raw_ref<typename MaybeProvidesType<T>::Type> field2;
};
