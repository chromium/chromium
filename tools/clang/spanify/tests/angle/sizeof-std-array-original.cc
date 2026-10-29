// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `sizeof(c_array)` is kept as is: the size of the `std::array` is the same,
// and no helper header is needed.

#include <cstring>

void Clear(int index) {
  // Expected rewrite:
  // std::array<int, 4> buffer = {};
  int buffer[4] = {};
  buffer[index] = 1;
  // Expected rewrite:
  // memset(buffer.data(), 0, sizeof(buffer));
  memset(buffer, 0, sizeof(buffer));
}
