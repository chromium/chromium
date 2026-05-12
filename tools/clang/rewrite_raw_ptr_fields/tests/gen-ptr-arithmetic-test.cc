// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace my_namespace {

struct MyStruct {
  int* ptr_arithmetic_plus;
  int* ptr_arithmetic_minus;
  int* ptr_arithmetic_plus_equal;
  int* ptr_arithmetic_minus_equal;
  int* ptr_arithmetic_increment;
  int* ptr_arithmetic_decrement;
  int* ptr_arithmetic_subscript;
  int* ptr_no_arithmetic;
};

void foo() {
  MyStruct s;
  int* p1 = s.ptr_arithmetic_plus + 1;
  int* p2 = s.ptr_arithmetic_minus - 1;
  s.ptr_arithmetic_plus_equal += 1;
  s.ptr_arithmetic_minus_equal -= 1;
  s.ptr_arithmetic_increment++;
  s.ptr_arithmetic_decrement--;
  int v = s.ptr_arithmetic_subscript[1];
  int* p3 = s.ptr_no_arithmetic;
}

}  // namespace my_namespace
