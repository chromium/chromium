// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <tuple>

struct A {
  int a;
  int b;
};

A aggregate_with_very_very_very_long_name = {1, 2};

int UnsafeIndex();

void Test() {
  // Expected rewrite:
  // auto a_1 = std::to_array<A, 1>({{1, 2}});
  auto a_1 = std::to_array<A, 1>({{1, 2}});
  std::ignore = a_1[UnsafeIndex()];

  // Expected rewrite:
  // auto a_2 = std::to_array<A, 2>({{1, 2}, {3, 4}});
  auto a_2 = std::to_array<A, 2>({{1, 2}, {3, 4}});
  std::ignore = a_2[UnsafeIndex()];

  // Expected rewrite:
  // auto a_2_long =
  //     std::to_array<A, 2>({aggregate_with_very_very_very_long_name,
  //                          aggregate_with_very_very_very_long_name});
  auto a_2_long =
      std::to_array<A, 2>({aggregate_with_very_very_very_long_name,
                           aggregate_with_very_very_very_long_name});
  std::ignore = a_2_long[UnsafeIndex()];

  // Expected rewrite:
  // auto a_5 = std::to_array<A, 5>({
  //     {1, 2},
  //     {3, 4},
  //     {5, 6},
  //     {7, 8},
  //     {9, 10},
  // });
  auto a_5 = std::to_array<A, 5>({
      {1, 2},
      {3, 4},
      {5, 6},
      {7, 8},
      {9, 10},
  });
  std::ignore = a_5[UnsafeIndex()];

  // Expected rewrite:
  // auto a_5_long = std::to_array<A, 5>({
  //     {1, 2},
  //     {3, 4},
  //     aggregate_with_very_very_very_long_name,
  //     {7, 8},
  //     {9, 10},
  // });
  auto a_5_long = std::to_array<A, 5>({
      {1, 2},
      {3, 4},
      aggregate_with_very_very_very_long_name,
      {7, 8},
      {9, 10},
  });
  std::ignore = a_5_long[UnsafeIndex()];

  // Expected rewrite:
  // auto a_6 = std::to_array<A, 6>({
  //     {1, 2},
  //     {3, 4},
  //     {5, 6},
  //     {7, 8},
  //     {9, 10},
  //     {11, 12},
  // });
  auto a_6 = std::to_array<A, 6>({
      {1, 2},
      {3, 4},
      {5, 6},
      {7, 8},
      {9, 10},
      {11, 12},
  });
  std::ignore = a_6[UnsafeIndex()];

  // Expected rewrite:
  // auto a_6_with_new_line = std::to_array<A, 6>({
  //     {1, 2},
  //     {3, 4},
  //     {5, 6},
  //     {7, 8},
  //     {9, 10},
  //     {11, 12},
  // });
  auto a_6_with_new_line = std::to_array<A, 6>({
      {1, 2},
      {3, 4},
      {5, 6},
      {7, 8},
      {9, 10},
      {11, 12},
  });
  std::ignore = a_6_with_new_line[UnsafeIndex()];

  // Expected rewrite:
  // auto a_5_with_trailing_comma = std::to_array<A, 5>({
  //     {1, 2},
  //     {3, 4},
  //     {5, 6},
  //     {7, 8},
  //     {9, 10},
  // });
  auto a_5_with_trailing_comma = std::to_array<A, 5>({
      {1, 2},
      {3, 4},
      {5, 6},
      {7, 8},
      {9, 10},
  });
  std::ignore = a_5_with_trailing_comma[UnsafeIndex()];

  // Expected rewrite:
  // auto a_5_with_trailing_comma_2 = std::to_array<A, 5>({
  //     {1, 2},
  //     {3, 4},
  //     {5, 6},
  //     {7, 8},
  //     {9, 10},
  // });
  auto a_5_with_trailing_comma_2 = std::to_array<A, 5>({
      {1, 2},
      {3, 4},
      {5, 6},
      {7, 8},
      {9, 10},
  });
  std::ignore = a_5_with_trailing_comma_2[UnsafeIndex()];
}
