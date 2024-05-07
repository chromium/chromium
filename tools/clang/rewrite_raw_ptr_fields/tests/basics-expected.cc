// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"

class SomeClass;

class MyClass {
  MyClass(SomeClass& s) : raw_ref_field(s) {}
  // Expected rewrite: raw_ptr<SomeClass> raw_ptr_field;
  raw_ptr<SomeClass> raw_ptr_field;

  // Expected rewrite: const raw_ref<SomeClass> raw_ref_field;
  const raw_ref<SomeClass> raw_ref_field;

  // No rewrite expected.
  int int_field;

  // Expected rewrite: base::raw_span<SomeClass> span_field;
  base::raw_span<SomeClass> span_field;

  // Expected rewrite: base::raw_span<SomeClass> span_field;
  std::vector<base::raw_span<SomeClass>> container_of_span_field;

  // Expected rewrite:
  // std::vector<std::optional<base::raw_span<SomeClass>>>
  std::vector<std::optional<base::raw_span<SomeClass>>>
      container_of_optional_span_field;

  // Expected rewrite: base::raw_span<SomeClass> span_field;
  base::raw_span<SomeClass> base_span_field;

  // No rewrite expected.
  base::raw_span<SomeClass> base_raw_span_field;

  // Expected rewrite: std::optional<base::raw_span<SomeClass>>
  // optional_span_field;
  std::optional<base::raw_span<SomeClass>> optional_span_field;

  // Expected rewrite: std::map<int, base::raw_span<SomeClass>> map_span_field1;
  std::map<int, base::raw_span<SomeClass>> map_span_field1;

  // Expected rewrite: std::map<base::raw_span<SomeClass>, int> map_span_field2;
  std::map<base::raw_span<SomeClass>, int> map_span_field2;

  // Expected rewrite: std::map<base::raw_span<SomeClass>,
  // base::raw_span<SomeClass>> map_span_field3;
  std::map<base::raw_span<SomeClass>, base::raw_span<SomeClass>>
      map_span_field3;

  // Expected rewrite:
  // std::map<base::span<const char>, base::raw_span<SomeClass>>
  std::map<base::span<const char>, base::raw_span<SomeClass>> map_span_field4;

  // Expected rewrite:
  // std::map<base::raw_span<SomeClass>, base::span<const char>>
  std::map<base::raw_span<SomeClass>, base::span<const char>> map_span_field5;

  // These fields are not expected to be rewritten.
  // Commented out due to presubmit failures (char8_t is not allowed in chromium
  // code yet).
  // base::span<const char8_t> const_char8_span;
  // std::optional<base::span<const char8_t>> optional_const_char8_span;
  // std::vector<base::span<const char8_t>> vector_const_char8_span;
  // std::vector<std::optional<base::span<const char8_t>>>
  // vector_optional_const_char8_span;

  // No rewrite expected for the following fields;
  base::span<const char> const_char_span;
  base::span<const wchar_t> const_wchar_span;
  base::span<const char16_t> const_char16_span;
  base::span<const char32_t> const_char32_span;
  std::optional<base::span<const char>> optional_const_char_span;
  std::optional<base::span<const wchar_t>> optional_const_wchar_span;
  std::optional<base::span<const char16_t>> optional_const_char16_span;
  std::optional<base::span<const char32_t>> optional_const_char32_span;
  std::vector<base::span<const char>> vector_const_char_span;
  std::vector<base::span<const wchar_t>> vector_const_wchar_span;
  std::vector<base::span<const char16_t>> vector_const_char16_span;
  std::vector<base::span<const char>> vector_const_char32_span;
  std::vector<std::optional<base::span<const char>>>
      vector_optional_const_char_span;
  std::vector<std::optional<base::span<const wchar_t>>>
      vector_optional_const_wchar_span;
  std::vector<std::optional<base::span<const char16_t>>>
      vector_optional_const_char16_span;
  std::vector<std::optional<base::span<const char32_t>>>
      vector_optional_const_char32_span;

  // All of the following fields are expected to be rewritten:
  // Expected rewrite: base::raw_span<const type>
  base::raw_span<const uint8_t> const_uint8_span;
  base::raw_span<const int8_t> const_int8_span;
  base::raw_span<const uint16_t> const_uint16_span;
  base::raw_span<const int16_t> const_int16_span;
  base::raw_span<const uint32_t> const_uint32_span;
  base::raw_span<const int32_t> const_int32_span;

  // All of the following fields are expected to be rewritten:
  // Expected rewrite:  std::optional<base::raw_span<const type>>
  std::optional<base::raw_span<const uint8_t>> optional_const_uint8_span;
  std::optional<base::raw_span<const int8_t>> optional_const_int8_span;
  std::optional<base::raw_span<const uint16_t>> optional_const_uint16_span;
  std::optional<base::raw_span<const int16_t>> optional_const_int16_span;
  std::optional<base::raw_span<const uint32_t>> optional_const_uint32_span;
  std::optional<base::raw_span<const int32_t>> optional_const_int32_span;

  // All of the following fields are expected to be rewritten:
  // Expected rewrite:
  // std::vector<base::raw_span<const type>>
  std::vector<base::raw_span<const uint8_t>> vector_const_uint8_span;
  std::vector<base::raw_span<const int8_t>> vector_const_int8_span;
  std::vector<base::raw_span<const uint16_t>> vector_const_uint16_span;
  std::vector<base::raw_span<const int16_t>> vector_const_int16_span;
  std::vector<base::raw_span<const uint32_t>> vector_const_uint32_span;
  std::vector<base::raw_span<const int32_t>> vector_const_int32_span;

  // All of the following fields are expected to be rewritten:
  // Expected rewrite:
  // std::vector<std::optional<base::raw_span<const type>>>
  std::vector<std::optional<base::raw_span<const uint8_t>>>
      vector_optional_const_uint8_span;
  std::vector<std::optional<base::raw_span<const int8_t>>>
      vector_optional_const_int8_span;
  std::vector<std::optional<base::raw_span<const uint16_t>>>
      vector_optional_const_uint16_span;
  std::vector<std::optional<base::raw_span<const int16_t>>>
      vector_optional_const_int16_span;
  std::vector<std::optional<base::raw_span<const uint32_t>>>
      vector_optional_const_uint32_span;
  std::vector<std::optional<base::raw_span<const int32_t>>>
      vector_optional_const_int32_span;
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

  // Expected rewrite: base::raw_span<T> span_field;
  base::raw_span<T> span_field;

  // Expected rewrite: std::vector<base::raw_span<T>> container_of_span_field;
  std::vector<base::raw_span<T>> container_of_span_field;

  // Expected rewrite: std::optional<base::raw_span<T>> optional_span_field;
  std::optional<base::raw_span<T>> optional_span_field;

  // Expected rewrite: std::optional<base::raw_span<T>> optional_span_field;
  std::optional<base::raw_span<int>> optional_int_span_field;

  // Expected rewrite: std::map<int, base::raw_span<T>> map_span_field1;
  std::map<int, base::raw_span<T>> map_span_field1;
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

  // Expected rewrite: base::raw_ptr<typename MaybeProvidesType<T>::Type>
  // span_field;
  base::raw_span<typename MaybeProvidesType<T>::Type> span_field;

  // Expected rewrite: std::vector<baes::raw_span<typename
  // MaybeProvidesType<T>::Type>> container_of_span_field;
  std::vector<base::raw_span<typename MaybeProvidesType<T>::Type>>
      container_of_span_field;

  // Expected rewrite: std::optional<base::raw_span<typename
  // MaybeProvidesType<T>::Type>> optional_span_field;
  std::optional<base::raw_span<typename MaybeProvidesType<T>::Type>>
      optional_span_field;

  // Expected rewrite: std::map<int,
  // base::raw_span<typenameMaybeProvidesType<T>::Type>> map_span_field;
  std::map<int, base::raw_span<typename MaybeProvidesType<T>::Type>>
      map_span_field;
};

namespace base {
using spancontainedtype = int;

struct S {
  // Expected rewrite:
  // raw_span<spancontainedtype> member;
  raw_span<spancontainedtype> member;
};
}  // namespace base
