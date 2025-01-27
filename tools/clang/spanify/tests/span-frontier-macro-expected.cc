// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

// When a C++ MACRO references a variable that isn't passed as an argument to
// that MACRO for example. It is sometimes not possible to spanify correctly.
// This documents a case where the spanification is aborted.

#define ASSIGN(num) assign(x, num);
void assign(int* x, int num) {
  *x = num;
}

void test_with_macro() {
  std::vector<int> buffer(10, 0);

  // A local variable could be rewritten to a span if there wasn't a MACRO.
  {
    int* x = buffer.data();
    x[0] = 0;
    ASSIGN(0);
  }

  // A local variable that doesn't need to be rewritten to a span.
  {
    int* x = buffer.data();
    ASSIGN(0);  // Sets the integer pointed to by x to 0;
  }
}
