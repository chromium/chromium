// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
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

// No function body so that the argument type won't get spanified.
void take_ptr1(int* arg);

// Macros that expect a pointer as the argument.
//
// TODO: Since these macros are defined and used inside this translation unit
// only, we may want to rewrite the macro definitions rather than adding
// ".data()" call on the call sites.
#define TAKE_PTR1(arg) (take_ptr1(arg))
#define TAKE_PTR2(arg) (arg + 1)
#define TAKE_PTR3_REINTERPRET_CAST(arg) (reinterpret_cast<unsigned char*>(arg))
#define TAKE_PTR3_STATIC_CAST(arg) (static_cast<const int*>(arg))
#define TAKE_PTR3_C_STYLE_CAST(arg) ((double*)(arg))

void test_take_ptr_macro() {
  int array[] = {1, 2, 3};
  // Expected rewrite:
  // base::span<int> buf = array;
  int* buf = array;
  buf[0] = 0;

  // Expected rewrite:
  // TAKE_PTR1(buf.data());
  TAKE_PTR1(buf);

  // Expected rewrite:
  // UNSAFE_TODO(TAKE_PTR2(buf.data()));
  TAKE_PTR2(buf);

  // Expected rewrite:
  // unsigned char* p1 = TAKE_PTR3_REINTERPRET_CAST(buf.data());
  unsigned char* p1 = TAKE_PTR3_REINTERPRET_CAST(buf);
  // Expected rewrite:
  // const int* p2 = TAKE_PTR3_STATIC_CAST(buf.data());
  const int* p2 = TAKE_PTR3_STATIC_CAST(buf);
  // Expected rewrite:
  // double* p3 = TAKE_PTR3_C_STYLE_CAST(buf.data());
  double* p3 = TAKE_PTR3_C_STYLE_CAST(buf);

  // The following rewrites are not compilable. Just demonstrating the current
  // behavior.
  //
  // Unexpected rewrite:
  // base::span<unsigned char> s1 = TAKE_PTR3_REINTERPRET_CAST(buf);
  unsigned char* s1 = TAKE_PTR3_REINTERPRET_CAST(buf);
  std::ignore = s1[0];
  // Unexpected rewrite:
  // base::span<const int> s2 = TAKE_PTR3_STATIC_CAST(buf);
  const int* s2 = TAKE_PTR3_STATIC_CAST(buf);
  std::ignore = s2[0];
  // Unexpected rewrite:
  // base::span<double> s3 = TAKE_PTR3_C_STYLE_CAST(buf);
  double* s3 = TAKE_PTR3_C_STYLE_CAST(buf);
  std::ignore = s3[0];

  // Just casting doesn't trigger rewriting.
  TAKE_PTR3_REINTERPRET_CAST(buf);
  TAKE_PTR3_STATIC_CAST(buf);
  TAKE_PTR3_C_STYLE_CAST(buf);
}
