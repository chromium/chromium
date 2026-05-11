// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

void f() {
  std::vector<int> ctn = {1, 2, 3, 4};
  // Expected rewrite:
  // angle::Span<int> ptr = ctn;
  int* ptr = ctn.data();
  ptr[0] = 0;
}
