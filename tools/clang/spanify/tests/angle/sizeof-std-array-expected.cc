// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `sizeof(c_array)` is kept as is: the size of the `std::array` is the same,
// and no helper header is needed.

#include <array>
#include <cstring>

void Clear(int index) {
  // Expected rewrite:
  // std::array<int, 4> buffer = {};
  std::array<int, 4> buffer = {};
  buffer[index] = 1;
  // Expected rewrite:
  // memset(buffer.data(), 0, sizeof(buffer));
  memset(buffer.data(), 0, sizeof(buffer));
}
