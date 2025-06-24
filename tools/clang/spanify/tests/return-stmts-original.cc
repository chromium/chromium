// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

// Expected rewrite:
// base::span<int> fct1()
int* fct1() {
  static std::vector<int> ctn = {1, 2, 3};
  // Expected rewrite:
  // return ctn;
  return ctn.data();
}

// Expected rewrite:
// base::span<int> fct2()
int* fct2() {
  static std::vector<int> ctn = {1, 2, 3};
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  // base::span<int> var2 = ctn;
  int* var2 = ctn.data();
  return (condition) ? var1 : var2;
}

// Expected rewrite:
// base::span<int> fct3()
int* fct3() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  // return base::PostIncrementSpan(var1);
  return var1++;
}

// Expected rewrite:
// base::span<int> fct3()
int* fct4() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  // return base::PreIncrementSpan(var1);
  return ++var1;
}

// Expected rewrite:
// base::span<int> fct5()
int* fct5() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return var1.subspan(base::checked_cast<size_t>(offset));
  return var1 + offset;
}

// Expected rewrite:
// base::span<char> fct6()
char* fct6() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return
  // base::as_writable_byte_span(var1.subspan(base::checked_cast<size_t>(offset)));
  return reinterpret_cast<char*>(var1 + offset);
}

// Function return type not rewritten since not used.
int* fct7() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return var1.subspan(base::checked_cast<size_t>(offset)).data();
  return var1 + offset;
}

// Function return type not rewritten since not used.
char* fct8() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  int* var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return
  // reinterpret_cast<char*>(var1).subspan(base::checked_cast<size_t>(offset)).data();
  // As-is, this code doesn't compile because we don't yet handle
  // adapting these reinterpret_cast expressions for spans.
  return reinterpret_cast<char*>(var1) + offset;
}

void usage() {
  // Expected rewrite:
  // base::span<int> v1 = fct1();
  int* v1 = fct1();
  // base::PostIncrementSpan(v1);
  v1++;

  // Expected rewrite:
  // base::span<int> v2 = fct2();
  int* v2 = fct2();
  // base::PostIncrementSpan(v2);
  v2++;

  // Expected rewrite:
  // base::span<int> v3 = fct3();
  int* v3 = fct3();
  // base::PostIncrementSpan(v3);
  v3++;

  // Expected rewrite:
  // base::span<int> v4 = fct4();
  int* v4 = fct4();
  // base::PostIncrementSpan(v4);
  v4++;

  // Expected rewrite:
  // base::span<int> v5 = fct5();
  int* v5 = fct5();
  // base::PostIncrementSpan(v5);
  v5++;

  // Expected rewrite:
  // base::span<char> v6 = fct6();
  char* v6 = fct6();
  // base::PostIncrementSpan(v6);
  v6++;
}
