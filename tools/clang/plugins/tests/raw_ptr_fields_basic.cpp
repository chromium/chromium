// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass {};

class MyClass {
 public:
  // Error expected.
  SomeClass* raw_ptr_field;

  // No error expected.
  int int_field;

  // Error expected.
  SomeClass& raw_ref_field;
};

struct MyStruct {
  // Error expected.
  SomeClass* raw_ptr_field;

  // No error expected.
  int int_field;

  // "*" next to the field name.  This is non-standard formatting, so
  // "clang-format off" is used to make sure |git cl format| won't change this
  // testcase.
  //
  // Error expected.
  // clang-format off
  SomeClass *raw_ptr_field2;
  // clang-format on

  // Error expected.
  SomeClass& raw_ref_field;

  // No error expected.
  int int_field2;

  // "&" next to the field name.  This is non-standard formatting, so
  // "clang-format off" is used to make sure |git cl format| won't change this
  // testcase.
  //
  // Error expected.
  // clang-format off
  SomeClass &raw_ref_field2;
  // clang-format on
};

template <typename T>
class MyTemplate {
 public:
  // Error expected.
  T* raw_ptr_field;

  // No error expected.
  int int_field;

  // Error expected.
  T& raw_ref_field;
};

// The field below won't compile without the |typename| keyword (because
// at this point we don't know if MaybeProvidesType<T>::Type is a type,
// value or something else).
template <typename T>
struct MaybeProvidesType;
template <typename T>
struct DependentNameTest {
  // Error expected.  Even though MaybeProvidesType<T>::Type is an unknown type,
  // MaybeProvidesType<Type::Type* must be a pointer so an error is expected.
  typename MaybeProvidesType<T>::Type* field;

  // Error expected.  Even though MaybeProvidesType<T>::Type is an unknown type,
  // MaybeProvidesType<Type::Type& must be a reference so an error is expected.
  typename MaybeProvidesType<T>::Type& ref_field;
};
