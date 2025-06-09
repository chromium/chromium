// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

// When a C++ MACRO references a variable that isn't passed as an argument to
// that MACRO for example. It is sometimes not possible to spanify correctly.
// This documents a case where the spanification is aborted.

#include "base/containers/span.h"

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

// No function body so that the argument type won't get spanified.
void take_ptr1(int* arg);

// A macro that expects a pointer as the argument.
#define TAKE_PTR1(arg) (take_ptr1(arg))

void test_take_ptr1_macro() {
  int array[3] = {1, 2, 3};
  // Expected rewrite:
  // base::span<int> buf = array;
  base::span<int> buf = array;
  buf[0] = 0;
  // Expected rewrite:
  // TAKE_PTR1(buf.data());
  TAKE_PTR1(buf.data());
}

// TODO(yukishiino): Support static_cast and C style cast.
// #define TAKE_PTR2(arg) ((char*)(arg))

// TODO(yukishiino): Support binary + operator.
// #define TAKE_PTR3(arg) (arg + 1)
