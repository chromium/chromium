// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>

void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4});
  auto buf = std::to_array<int>({1, 2, 3, 4});
  buf[1] = 1;

  // Expected rewrite:
  // auto buf2 = std::to_array<char>({'x', 'y', 'z'});
  auto buf2 = std::to_array<char>({'x', 'y', 'z'});
  buf2[1] = 'a';

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
  data[1] = 'a';
  File f;
  // Expected rewrite:
  // f.ReadAtCurrentPos(data.data(), 10);
  f.ReadAtCurrentPos(data.data(), 10);
}

void fct3() {
  // Expected rewrite:
  // std::array<char, 10> data;
  std::array<char, 10> data;
  data[1] = 'a';
  // No rewrite expected. This is because std::size() etc. accepts std::array.
  std::ignore = std::size(data);
  std::ignore = std::begin(data);
  std::ignore = std::end(data);
  std::ignore = std::empty(data);
  std::swap(data, data);
  std::ranges::find(data, 'a');
  std::ignore = std::ranges::min(data);
}
