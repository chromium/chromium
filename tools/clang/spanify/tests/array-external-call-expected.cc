// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>

int UnsafeIndex();  // This function might return an out-of-bound index.

void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4});
  auto buf = std::to_array<int>({1, 2, 3, 4});
  buf[UnsafeIndex()] = 1;

  // Expected rewrite:
  // auto buf2 = std::to_array<char>({'x', 'y', 'z'});
  auto buf2 = std::to_array<char>({'x', 'y', 'z'});
  buf2[UnsafeIndex()] = 'a';

  // Expected rewrite:
  // memcpy(buf2.data(), buf.data(), 1);
  memcpy(buf2.data(), buf.data(), 1);
}

#define UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]

class File {
 public:
  File() = default;
  // No rewrite expected because this has UNSAFE_BUFFER_USAGE.
  UNSAFE_BUFFER_USAGE int ReadAtCurrentPos(char* data, int size);
};

void fct2() {
  // Expected rewrite:
  // std::array<char, 10> data;
  std::array<char, 10> data;
  data[UnsafeIndex()] = 'a';
  File f;
  // Expected rewrite:
  // f.ReadAtCurrentPos(data.data(), 10);
  f.ReadAtCurrentPos(data.data(), 10);
}

void fct3() {
  // Expected rewrite:
  // std::array<char, 10> data;
  std::array<char, 10> data;
  data[UnsafeIndex()] = 'a';
  // No rewrite expected. This is because std::size() etc. accepts std::array.
  std::ignore = std::size(data);
  std::ignore = std::begin(data);
  std::ignore = std::end(data);
  std::ignore = std::cbegin(data);
  std::ignore = std::cend(data);
  std::ignore = std::rbegin(data);
  std::ignore = std::rend(data);
  std::ignore = std::crbegin(data);
  std::ignore = std::crend(data);
  std::ignore = std::empty(data);
  std::swap(data, data);
  std::ranges::find(data, 'a');
  std::ignore = std::ranges::min(data);
}

void fct4() {
  // Adding .data() works for std::string_view rewrites too.
  // Expected rewrite:
  // const std::string_view buf = "123456789";
  const std::string_view buf = "123456789";
  std::ignore = buf[UnsafeIndex()];
  // Expected rewrite:
  // std::ignore = memcmp(buf.data(), "xxx456789", 3);
  std::ignore = memcmp(buf.data(), "xxx456789", 3);
}

void fct5() {
  // TODO(crbug.com/400492894): Rewrite the following code to compilable code.
  // Expected rewrite:
  // auto buf = std::to_array<char>({"hello, world"});
  auto buf = std::to_array<char>({"hello, world"});
  std::ignore = buf[UnsafeIndex()];
  // Expected rewrite:
  // std::string s(buf.data());
  std::string s(buf.data());
}
