// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

int UnsafeIndex();  // This function might return an out-of-bound index.

// Scenario 1: constexpr const char[] initialized with a string literal.
void test_constexpr_const_char_array() {
  // Expected rewrite:
  // static constexpr std::string_view kFoo = "foo";
  static constexpr const char kFoo[] = "foo";
  std::string bar = "bar1";

  // String concatenation should not emit a source node.
  // No rewrite expected.
  std::ignore = kFoo + bar;
}

void test_constexpr_const_char_array2() {
  // Expected rewrite:
  // static constexpr std::string_view kFoo = "foo";
  static constexpr const char kFoo[] = "foo";
  std::string bar = "bar1";

  // Expected rewrite:
  // std::ignore =
  // kFoo.subspan(base::checked_cast<size_t>(UnsafeIndex())).data();
  std::ignore = kFoo + UnsafeIndex();
}

// Scenario 2: constexpr char[] initialized with a string literal.
void test_constexpr_char_array() {
  // Expected rewrite:
  // static constexpr std::string_view kFoo = "foo";
  static constexpr char kFoo[] = "foo";
  std::string bar = "bar2";

  // String concatenation should not emit a source node.
  // No rewrite expected.
  std::ignore = kFoo + bar;
}

void test_constexpr_char_array2() {
  // Expected rewrite:
  // static constexpr std::string_view kFoo = "foo";
  static constexpr char kFoo[] = "foo";
  std::string bar = "bar2";

  // Expected rewrite:
  // std::ignore =
  // kFoo.subspan(base::checked_cast<size_t>(UnsafeIndex())).data();
  std::ignore = kFoo + UnsafeIndex();
}

// Scenario 3: Non-const char[] initialized with a string literal.
void test_mutable_char_array() {
  // Expected rewrite:
  // std::array<char, 4> kFoo{"foo"};
  char kFoo[] = "foo";
  std::string bar = "bar3";

  // String concatenation should not emit a source node.
  // No rewrite expected.
  std::ignore = kFoo + bar;
}

void test_mutable_char_array2() {
  // Expected rewrite:
  // std::array<char, 4> kFoo{"foo"};
  char kFoo[] = "foo";
  std::string bar = "bar3";

  // Expected rewrite:
  // std::ignore = base::span<char>(kFoo).subspan(
  //   base::checked_cast<size_t>(UnsafeIndex())).data();
  std::ignore = kFoo + UnsafeIndex();
}
