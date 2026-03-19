// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

// A macro obscures the source location of the operator
// No expected rewrite, macros are not supported yet because
// spanifier will generate closing parenthesis in unexpected locations
#define PRE_INC(x) ++x

base::span<int> test_macro_increment(base::span<int, 3> buf) {
  base::span<int> ptr = buf;
  // Regression test for "++i" crashing when written inside macros.
  PRE_INC(ptr);
  return ptr;
}

int main() {
  int buf[3] = {1, 2, 3};
  base::span<int> ptr = test_macro_increment(buf);
  return ptr[0];
}
