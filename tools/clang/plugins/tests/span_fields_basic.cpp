// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "base/containers/span.h"

class SomeClass {};

// No error expected. Only class/struct members are enforced.
int get(base::span<int> s) {
  return 0;
}

// Since base::span internally stores a pointer, base::span class/struct members
// should internally store a raw_ptr. For this reason base::raw_span should be
// used instead of base::span for class/struct members.
class MyClass {
 public:
  using IntSpan = base::span<int>;
  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<SomeClass> member1;

  // No error expected.
  int int_field;

  // Error expected.
  // IntSpan is an alias for base::span, base::raw_span should be used instead
  // of base::span for class/struct members.
  IntSpan member2;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  std::vector<base::span<SomeClass>> member3;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members (including container template arguments).
  std::map<int, base::span<SomeClass>> member4;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members (including container template arguments).
  std::map<base::span<SomeClass>, SomeClass*> member5;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members (including container template arguments).
  std::map<base::span<SomeClass>, base::span<SomeClass>> member6;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<SomeClass> member7;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members (including container template arguments).
  std::vector<base::span<SomeClass>> member8;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  std::optional<base::span<SomeClass>> member9;

  // No error expected. Already using base::raw_span
  base::raw_span<SomeClass> raw_span_member;

  // No error expected. Already using base::raw_span
  std::vector<base::raw_span<SomeClass>> raw_span_container;

  // No error expected. Already using base::raw_span
  std::optional<base::raw_span<SomeClass>> optional_raw_span;

  // No errors expected for the following fields.
  // const char*, const wchar_t*, const char8_t*, const char16_t*, const
  // char32_t* are excluded from the rewrite because there are likely pointing
  // to string literal, which wouldn't be protected by MiraclePtr.
  base::span<const char> const_char_span_field;
  base::span<const wchar_t> const_wchar_span_field;
  base::span<const char8_t> const_char8_span_field;
  base::span<const char16_t> const_char16_span_field;
  base::span<const char32_t> const_char32_span_field;
  std::optional<base::span<const char>> optional_const_char_span_field;
  std::optional<base::span<const wchar_t>> optional_const_wchar_span_field;
  std::optional<base::span<const char8_t>> optional_const_char8_span_field;
  std::optional<base::span<const char16_t>> optional_const_char16_span_field;
  std::optional<base::span<const char32_t>> optional_const_char32_span_field;
  std::vector<base::span<const char>> vector_const_char_span_field;
  std::vector<base::span<const wchar_t>> vector_const_wchar_span_field;
  std::vector<base::span<const char8_t>> vector_const_char8_span_field;
  std::vector<base::span<const char16_t>> vector_const_char16_span_field;
  std::vector<base::span<const char32_t>> vector_const_char32_span_field;

  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<char> char_span;
  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<wchar_t> wide_char_span;
  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<char8_t> char8_span;
  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<char16_t> char16_span;
  // Error expected.
  // base::raw_span should be used instead of base::span for class/struct
  // members.
  base::span<char32_t> char32_span;
  // Error expected. const uint8_t pointers don't point to string literals.
  // base::raw_span should be used instead of base::span
  base::span<const uint8_t> const_uint8_span;
  // Error expected. const int8_t pointers don't point to string literals.
  // base::raw_span should be used instead of base::span
  base::span<const int8_t> const_int8_span;
  // Error expected. const unsigned char don't point to string literals.
  // base::raw_span should be used instead of base::span
  base::span<const unsigned char> const_unsigned_char_span;
  // Error expected. const signed char pointers don't point to string literals.
  // base::raw_span should be used instead of base::span
  base::span<const signed char> const_signed_char_span;
};

// The field below won't compile without the |typename| keyword (because
// at this point we don't know if MaybeProvidesType<T>::Type is a type,
// value or something else).
template <typename T>
struct MaybeProvidesType;
template <typename T>
struct DependentNameTest {
  // Error expected.
  // base::raw_span should be used instead of base::span for members.
  base::span<typename MaybeProvidesType<T>::Type> span_field;
  // Error expected.
  // base::raw_span should be used instead of base::span for  members.
  std::vector<base::span<typename MaybeProvidesType<T>::Type>>
      vector_of_span_field;
  // Error expected.
  // base::raw_span should be used instead of base::span for members.
  std::optional<base::span<typename MaybeProvidesType<T>::Type>>
      optional_span_field;

  // Error expected.
  // base::raw_span should be used instead of base::span for members.
  std::vector<std::optional<base::span<typename MaybeProvidesType<T>::Type>>>
      vector_of_optional_span_field;
};
