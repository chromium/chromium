// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <array>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

// Expected rewrite:
// base::span<T> get()
template <typename T>
base::span<T> get() {
  // Expected rewrite:
  // return {};
  return {};
}

void fct() {
  int index = 1;
  int value = 11;
  bool condition = false;
  std::vector<int> ctn1 = {1, 2, 3, 4};
  std::array<int, 3> ctn2 = {5, 6, 7};

  // Expected rewrite:
  // base::span<int> a = ctn1;
  base::span<int> a = ctn1;
  // Expected rewrite:
  // base::span<int> b = ctn2;
  base::span<int> b = ctn2;

  if (condition) {
    b = a;
  }
  // Expected rewrite:
  // base::span<int> c = b;
  base::span<int> c = b;
  c[index] = value;  // Buffer usage, leads c to be rewritten.

  // Expected rewrite:
  // base::raw_span<int> d = c;
  base::raw_span<int> d = c;

  d[index] = value;  // Buffer usage, leads d to be rewritten.

  // Expected rewrite:
  // base::span<int> e = d;
  base::span<int> e = d;

  e++;  // Buffer usage, leads e to be rewritten.

  // Expected rewrite:
  // base::span<int> f = get<int>();
  base::span<int> f = get<int>();

  ++f;  // Leads to f being rewritten.

  // Exptected rewrite:
  // base::span<int> g = (condition) ? ctn1 : ctn2;
  base::span<int> g = (condition) ? ctn1 : ctn2;

  g += 1;  // buffer udage: leads g to be rewritten.

  // Expected rewrite:
  // base::span<char> h = reinterpret_cast<char*>(g);
  base::span<char> h = reinterpret_cast<char*>(g);
  h[index] = 'x';
}

// Expected rewrite:
// void some_fct(int limit, base::span<int> buf)
void some_fct(int limit, base::span<int> buf) {
  for (int i = 0; i < limit; i++) {
    buf[i] = 'c';
  }
  // Expected rewrite:
  // buf[0] = 0;
  buf[0] = 0;
}

void raw_ptr_variables() {
  int buf[10];
  // Expected rewrite:
  // base::span<int> ptr = buf;
  base::span<int> ptr = buf;
  (void)ptr[1];

  // Expected rewrite:
  // int* ptr2 = ptr.data();
  int* ptr2 = ptr.data();
  (void)ptr2;

  // Expected rewrite:
  // base::span<char> ptr3;
  base::span<char> ptr3;
  // Expected rewrite:
  // ptr3 = {};
  ptr3 = {};
  (void)ptr3[1];

  some_fct(1, buf);
  // Expected rewrite:
  // some_fct(ptr3[0], buf);
  some_fct(ptr3[0], buf);

  // Expected rewrite:
  // base::span<char> buf2 = new char[5];
  base::span<char> buf2 = new char[5];
  buf2 += 1;

  // Expected rewrite:
  // base::raw_span<char> buf3 = buf2;
  base::raw_span<char> buf3 = buf2;
  buf3 += 1;

  // Expected rewrite:
  // base::raw_span<char> buf4 = buf3;
  base::base::raw_span<char> buf4 = buf3;
  buf4++;

  // Expected rewrite:
  // base::raw_span<char> buf5 = buf4;
  base::raw_span<char> buf5 = buf4;
  buf5 = buf5 + 1;

  // Expected rewrite:
  // base::raw_span<char> buf6 = buf5;
  base::raw_span<char> buf6 = buf5;
  ++buf6;

  // Expected rewrite:
  // buf6[0] = 'c';
  buf6[0] = 'c';

  int index = 1;
  // Expected rewrite:
  // raw_ptr<char> buf7 = (buf6 + index).data();
  raw_ptr<char> buf7 = (buf6 + index).data();
  (void)buf7;
}
