// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <tuple>

// No rewrite expected.
extern const int kPropertyVisitedIDs[];

namespace ns1 {

struct Type1 {
  int value;
};

}  // namespace ns1

void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4});
  int buf[] = {1, 2, 3, 4};
  int index = 0;
  buf[index] = 11;

  // Expected rewrite:
  // std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  int buf2[5] = {1, 1, 1, 1, 1};
  buf2[index] = 11;

  constexpr int size = 5;
  // Expected rewrite:
  // constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  constexpr int buf3[size] = {1, 1, 1, 1, 1};
  (void)buf3[index];

  // Expected rewrite:
  // std::array<int, buf3[0]> buf4;
  int buf4[buf3[0]];
  buf4[index] = 11;

  // Expected rewrite:
  // auto buf5 = std::to_array<ns1::Type1>({{1}, {1}, {1}, {1}, {1}});
  ns1::Type1 buf5[] = {{1}, {1}, {1}, {1}, {1}};
  buf5[index].value = 11;

  // Expected rewrite:
  // auto buf6 = std::to_array<uint16_t>({1, 1, 1});
  uint16_t buf6[] = {1, 1, 1};
  buf6[index] = 1;

  // Expected rewrite:
  // std::array<int (*)(int), 16> buf7 = {};
  int (*buf7[16])(int) = {};
  buf7[index] = nullptr;

  // Expected rewrite:
  // std::array<int(**)[], 16> buf8 = {};
  int(**buf8[16])[] = {};
  buf8[index] = nullptr;

  using Arr = int(**)[];
  // Expected rewrite:
  // std::array<Arr, buf3[0]> buf9 = {};
  Arr buf9[buf3[0]] = {};
  buf9[index] = nullptr;

  // Expected rewrite:
  // static auto buf10 = std::to_array<const volatile char*>({"1", "2", "3"});
  volatile static const char* buf10[] = {"1", "2", "3"};
  buf10[index] = nullptr;

  index = kPropertyVisitedIDs[index];
}

void sizeof_array_expr() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1});
  int buf[]{1};
  std::ignore = buf[0];

  // Expected rewrite:
  // std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  std::ignore = sizeof buf;
  // Expected rewrite:
  // std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  std::ignore = sizeof(buf);
  // The following won't be rewritten.
  std::ignore = sizeof *buf;
  std::ignore = sizeof buf[0];
}
