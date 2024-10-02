// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // std::array<Aggregate, 3> buf0 = {{{13, 1, 7}, {14, 2, 5}, {15, 2, 4}}};
  Aggregate buf0[] = {{13, 1, 7}, {14, 2, 5}, {15, 2, 4}};
  buf0[index].a = 0;

  // Expected rewrite:
  // std::array<Aggregate, 2> buf1 = {
  //     Build(1, 2, 3),
  //     Build(4, 5, 6),
  // };
  Aggregate buf1[2] = {
      Build(1, 2, 3),
      Build(4, 5, 6),
  };
  buf1[index].a = 0;

  // Expected rewrite:
  // std::array<Aggregate, 3> buf2 = {{
  //     Build(1, 2, 3),
  //     {1, 2, 3},
  //     Build(4, 5, 6),
  // }};
  Aggregate buf2[3] = {
      Build(1, 2, 3),
      {1, 2, 3},
      Build(4, 5, 6),
  };
  buf2[index].a = 0;
}

void test_with_arrays() {
  // Expected rewrite:
  // std::array<int[3], 3> buf0 = {{{0, 1, 2}, {3, 4, 5}, {6, 7, 8}}};
  int buf0[3][3] = {
      {0, 1, 2},
      {3, 4, 5},
      {6, 7, 8},
  };
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
  // std::array<std::string, 3> buf0 = {"1", "2", "3"};
  std::string buf0[] = {"1", "2", "3"};
  buf0[index] = "4";
}
