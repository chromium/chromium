// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

namespace {

struct Aggregate {
  int a;
  int b;
  int c;
};

Aggregate Build(int a, int b, int c) {
  return Aggregate{a, b, c};
}

}  // namespace

void test_with_structs() {
  const int index = 0;

  // Expected rewrite:
  // auto buf0 = std::to_array<Aggregate>({{13, 1, 7}, {14, 2, 5}, {15, 2, 4}});
  auto buf0 = std::to_array<Aggregate>({{13, 1, 7}, {14, 2, 5}, {15, 2, 4}});
  buf0[index].a = 0;

  // Expected rewrite:
  // auto buf1 = std::to_array<Aggregate, 2>({
  //     Build(1, 2, 3),
  //     Build(4, 5, 6),
  // });
  auto buf1 = std::to_array<Aggregate, 2>({
      Build(1, 2, 3),
      Build(4, 5, 6),
  });
  buf1[index].a = 0;

  // Expected rewrite:
  // auto buf2 = std::to_array<Aggregate, 3>({
  //     Build(1, 2, 3),
  //     {1, 2, 3},
  //     Build(4, 5, 6),
  // });
  auto buf2 = std::to_array<Aggregate, 3>({
      Build(1, 2, 3),
      {1, 2, 3},
      Build(4, 5, 6),
  });
  buf2[index].a = 0;
}

void test_with_arrays() {
  // Expected rewrite:
  // auto buf0 = std::to_array<std::array<int, 3>, 3>({
  //     {0, 1, 2},
  //     {3, 4, 5},
  //     {6, 7, 8},
  // });
  auto buf0 = std::to_array<std::array<int, 3>, 3>({
      {0, 1, 2},
      {3, 4, 5},
      {6, 7, 8},
  });
  buf0[0][0] = 0;

  // Since function returning array is not allowed, we don't need to
  // test the following:
  //   int buf1[3][3] = {
  //      BuildArray(0, 1, 2)
  //      BuildArray(3, 4, 5)
  //      BuildArray(6, 7, 8)
  //   };
}

void test_with_strings() {
  const int index = 0;
  // Expected rewrite:
  // auto buf0 = std::to_array<std::string>({"1", "2", "3"});
  auto buf0 = std::to_array<std::string>({"1", "2", "3"});
  buf0[index] = "4";
}

void test_with_const() {
  // Expected rewrite:
  // const auto data = std::to_array<bool>({false, true});
  const auto data = std::to_array<bool>({false, true});
  (void)data[0];
}

void test_with_static() {
  // Expected rewrite:
  // static auto data = std::to_array<int>({1, 2, 3});
  static auto data = std::to_array<int>({1, 2, 3});
  (void)data[0];
}

void test_with_constexpr() {
  // Expected rewrite:
  // constexpr const auto data = std::to_array<int>({1, 2, 3});
  constexpr const auto data = std::to_array<int>({1, 2, 3});
  (void)data[0];
}

void test_with_volatile() {
  // Expected rewrite:
  // auto data = std::to_array<volatile int>({1, 2, 3});
  auto data = std::to_array<volatile int>({1, 2, 3});
  (void)data[0];
}

void test_with_all_qualifiers() {
  // Expected rewrite:
  // static const auto data = std::to_array<volatile int>({1, 2, 3});
  static const auto data = std::to_array<volatile int>({1, 2, 3});
  (void)data[0];
}

void test_with_const_char() {
  // Expected rewrite:
  // static auto data = std::to_array<const char*>({" B", " kB", " MB"});
  static auto data = std::to_array<const char*>({" B", " kB", " MB"});
  (void)data[0];
}

void test_with_constant_const_char() {
  // Expected rewrite:
  // static const auto data = std::to_array<const char*>({" B", " kB", " MB"});
  static const auto data = std::to_array<const char*>({" B", " kB", " MB"});
  (void)data[0];
}
