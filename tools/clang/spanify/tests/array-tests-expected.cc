// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <cstring>
#include <tuple>

#include "base/containers/span.h"

// No rewrite expected.
extern const int kPropertyVisitedIDs[];

namespace ns1 {

struct Type1 {
  int value;
};

}  // namespace ns1

int UnsafeIndex();  // This function might return an out-of-bound index.

struct Component {
  // Expected rewrite:
  // std::array<int, 10> values;
  std::array<int, 10> values;
};

void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4});
  auto buf = std::to_array<int>({1, 2, 3, 4});
  buf[UnsafeIndex()] = 11;

  Component component;
  // Triggers the rewrite of Component::values
  component.values[UnsafeIndex()]++;

  // Expected rewrite:
  // std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  buf2[UnsafeIndex()] = 11;

  constexpr int size = 5;
  // Expected rewrite:
  // constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  (void)buf3[UnsafeIndex()];

  // Expected rewrite:
  // std::array<int, buf3[0]> buf4;
  std::array<int, buf3[0]> buf4;
  buf4[UnsafeIndex()] = 11;

  // Expected rewrite:
  // auto buf5 = std::to_array<ns1::Type1>({{1}, {1}, {1}, {1}, {1}});
  auto buf5 = std::to_array<ns1::Type1>({{1}, {1}, {1}, {1}, {1}});
  buf5[UnsafeIndex()].value = 11;

  // Expected rewrite:
  // auto buf6 = std::to_array<uint16_t>({1, 1, 1});
  auto buf6 = std::to_array<uint16_t>({1, 1, 1});
  buf6[UnsafeIndex()] = 1;

  // Expected rewrite:
  // std::array<int (*)(int), 16> buf7 = {};
  std::array<int (*)(int), 16> buf7 = {};
  buf7[UnsafeIndex()] = nullptr;

  // Expected rewrite:
  // std::array<int (**)[], 16> buf8 = {};
  std::array<int (**)[], 16> buf8 = {};
  buf8[UnsafeIndex()] = nullptr;

  using Arr = int (**)[];
  // Expected rewrite:
  // std::array<Arr, buf3[0]> buf9 = {};
  std::array<Arr, buf3[0]> buf9 = {};
  buf9[UnsafeIndex()] = nullptr;

  // Expected rewrite:
  // static auto buf10 = std::to_array<const volatile char*>({"1", "2", "3"});
  static auto buf10 = std::to_array<const volatile char*>({"1", "2", "3"});
  buf10[UnsafeIndex()] = nullptr;

  std::ignore = kPropertyVisitedIDs[UnsafeIndex()];
}

void sizeof_array_expr() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1});
  auto buf = std::to_array<int>({1});
  std::ignore = buf[UnsafeIndex()];

  // Expected rewrite:
  // std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  // Expected rewrite:
  // std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  std::ignore = (buf.size() * sizeof(decltype(buf)::value_type));
  // Expected rewrite:
  // std::ignore = sizeof buf[0];
  std::ignore = sizeof buf[0];
  std::ignore = sizeof buf[0];
}

// Test for crbug.com/383424943.
void crbug_383424943() {
  // No rewrite expected.
  int buf[]{1};
  // Using sizeof was causing buf to be rewritten.
  memset(buf, 'x', sizeof(buf));
}

// Expected rewrite:
// void c_ptr_param(base::span<int> ptr)
void c_ptr_param(base::span<int> ptr) {
  std::ignore = ptr[UnsafeIndex()];
}

// Expected rewrite:
// void c_array_param(base::span<int, 1 + 2> arr)
void c_array_param(base::span<int, 1 + 2> arr) {
  std::ignore = arr[UnsafeIndex()];
}

// Expected rewrite:
// void c_array_nosize_param(base::span<int> arr)
void c_array_nosize_param(base::span<int> arr) {
  std::ignore = arr[UnsafeIndex()];
}

void test_func_params() {
  // Expected rewrite:
  // auto arr = std::to_array<int>({1, 2, 3});
  auto arr = std::to_array<int>({1, 2, 3});
  std::ignore = arr[UnsafeIndex()];

  c_ptr_param(arr);
  c_array_param(arr);
  c_array_nosize_param(arr);
}

struct ProgramInfo {
  // Expected rewrite:
  // mutable std::array<int, 10> filename_offsets;
  mutable std::array<int, 10> filename_offsets;
};

void test_with_mutable() {
  const ProgramInfo info;
  info.filename_offsets[UnsafeIndex()] = 0xdead;
}
