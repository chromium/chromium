// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>

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
  auto buf = std::to_array<int>({1, 2, 3, 4});
  int index = 0;
  buf[index] = 11;

  // Expected rewrite:
  // auto buf2 = std::to_array<int, 5>({1, 1, 1, 1, 1});
  auto buf2 = std::to_array<int, 5>({1, 1, 1, 1, 1});
  buf2[index] = 11;

  constexpr int size = 5;
  // Expected rewrite:
  // constexpr auto buf3 = std::to_array<int, size>({1, 1, 1, 1, 1});
  constexpr auto buf3 = std::to_array<int, size>({1, 1, 1, 1, 1});
  (void)buf3[index];

  // Expected rewrite:
  // std::array<int, buf3[0]> buf4;
  std::array<int, buf3[0]> buf4;
  buf4[index] = 11;

  // Expected rewrite:
  // auto buf5 = std::to_array<ns1::Type1>({{1}, {1}, {1}, {1}, {1}});
  auto buf5 = std::to_array<ns1::Type1>({{1}, {1}, {1}, {1}, {1}});
  buf5[index].value = 11;

  // Expected rewrite:
  // auto buf6 = std::to_array<uint16_t>({1, 1, 1});
  auto buf6 = std::to_array<uint16_t>({1, 1, 1});
  buf6[index] = 1;

  // Expected rewrite:
  // auto buf7 = std::to_array<int (*)(int), 16>({nullptr});
  auto buf7 = std::to_array<int (*)(int), 16>({nullptr});
  buf7[index] = nullptr;

  // Expected rewrite:
  // auto buf8 = std::to_array<int(**)[], 16>({nullptr});
  auto buf8 = std::to_array<int(**)[], 16>({nullptr});
  buf8[index] = nullptr;

  using Arr = int(**)[];
  // Expected rewrite:
  // auto buf9 = std::to_array<Arr, buf3[0]>({nullptr});
  auto buf9 = std::to_array<Arr, buf3[0]>({nullptr});
  buf9[index] = nullptr;

  // Expected rewrite:
  // static auto buf10 = std::to_array<const volatile char*>({"1", "2", "3"});
  static auto buf10 = std::to_array<const volatile char*>({"1", "2", "3"});
  buf10[index] = nullptr;

  index = kPropertyVisitedIDs[index];
}
