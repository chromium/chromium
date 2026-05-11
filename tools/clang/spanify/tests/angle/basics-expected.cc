// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "common/span.h"

void f() {
  std::vector<int> ctn = {1, 2, 3, 4};
  // Expected rewrite:
  // angle::Span<int> ptr = ctn;
  angle::Span<int> ptr = ctn;
  ptr[0] = 0;
}
