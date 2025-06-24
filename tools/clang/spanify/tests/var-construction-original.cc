// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <array>
#include <vector>

#include "base/memory/raw_ptr.h"

// Expected rewrite:
// base::span<T> get()
template <typename T>
T* get() {
  // Expected rewrite:
  // return {};
  return nullptr;
}

void fct() {
  int index = 1;
  int value = 11;
  bool condition = false;
  std::vector<int> ctn1 = {1, 2, 3, 4};
  std::array<int, 3> ctn2 = {5, 6, 7};

  // Expected rewrite:
  // base::span<int> a = ctn1;
  int* a = ctn1.data();
  // Expected rewrite:
  // base::span<int> b = ctn2;
  int* b = ctn2.data();

  if (condition) {
    b = a;
  }
  // Expected rewrite:
  // base::span<int> c = b;
  int* c = b;
  c[index] = value;  // Buffer usage, leads c to be rewritten.

  // Expected rewrite:
  // base::raw_span<int> d = c;
  raw_ptr<int> d = c;

  d[index] = value;  // Buffer usage, leads d to be rewritten.

  // Expected rewrite:
  // base::span<int> e = d;
  int* e = d.get();

  // Expected rewrite:
  // base::PostIncrementSpan(e);
  e++;  // Buffer usage, leads e to be rewritten.

  // Expected rewrite:
  // base::span<int> f = get<int>();
  int* f = get<int>();

  // Expected rewrite:
  // base::PreIncrementSpan(f);
  ++f;  // Leads to f being rewritten.

  // Exptected rewrite:
  // base::span<int> g = (condition) ? ctn1 : ctn2;
  int* g = (condition) ? ctn1.data() : ctn2.data();

  // Buffer usage: leads `g` to be rewritten.
  // Expected rewrite:
  // g = g.subspan(1u);
  g += 1;

  // Expected rewrite:
  // base::span<char> h = base::as_writable_byte_span(g);
  char* h = reinterpret_cast<char*>(g);
  h[index] = 'x';
}

// Expected rewrite:
// void some_fct(int limit, base::span<int> buf)
void some_fct(int limit, int* buf) {
  for (int i = 0; i < limit; i++) {
    buf[i] = 'c';
  }
  // Expected rewrite:
  // buf[0] = 0;
  *buf = 0;
}

void raw_ptr_variables() {
  int buf[10];
  // Expected rewrite:
  // base::span<int> ptr = buf;
  int* ptr = buf;
  (void)ptr[1];

  // Expected rewrite:
  // int* ptr2 = ptr.data();
  int* ptr2 = ptr;
  (void)ptr2;

  // Expected rewrite:
  // base::span<char> ptr3;
  char* ptr3;
  // Expected rewrite:
  // ptr3 = {};
  ptr3 = nullptr;
  (void)ptr3[1];

  some_fct(1, buf);
  // Expected rewrite:
  // some_fct(ptr3[0], buf);
  some_fct(*ptr3, buf);

  // Expected rewrite:
  // base::span<char> buf2 = new char[5];
  // buf2 = buf2.subspan(1);
  char* buf2 = new char[5];
  // Expected rewrite:
  // buf2 = buf2.subspan(1u);
  buf2 += 1;

  // Expected rewrite:
  // base::raw_span<char> buf3 = buf2;
  // buf3 = buf3.subspan(1);
  raw_ptr<char> buf3 = buf2;
  // Expected rewrite:
  // buf3 = buf3.subspan(1u);
  buf3 += 1;

  // Expected rewrite:
  // base::raw_span<char> buf4 = buf3;
  // base::PostIncrementSpan(buf4);
  base::raw_ptr<char> buf4 = buf3;
  buf4++;

  // Expected rewrite:
  // base::raw_span<char> buf5 = buf4;
  // buf5 = buf5.subspan(1);
  raw_ptr<char> buf5 = buf4;
  // Expected rewrite:
  // buf5 = buf5.subspan(1u);
  buf5 = buf5 + 1;

  // Expected rewrite:
  // base::raw_span<char> buf6 = buf5;
  // base::PreIncrementSpan(buf6);
  raw_ptr<char> buf6 = buf5;
  ++buf6;

  // Expected rewrite:
  // buf6[0] = 'c';
  *buf6 = 'c';

  int index = 1;
  // Expected rewrite:
  // raw_ptr<char> buf7 =
  // buf6.subspan(base::checked_cast<size_t>(index)).data();
  raw_ptr<char> buf7 = buf6 + index;
  (void)buf7;
}
