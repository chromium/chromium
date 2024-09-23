// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/containers/span.h"

// Expected rewrite:
// base::span<int> fct1()
base::span<int> fct1() {
  static std::vector<int> ctn = {1, 2, 3};
  // Expected rewrite:
  // return ctn;
  return ctn;
}

// Expected rewrite:
// base::span<int> fct2()
base::span<int> fct2() {
  static std::vector<int> ctn = {1, 2, 3};
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  // base::span<int> var2 = ctn;
  base::span<int> var2 = ctn;
  return (condition) ? var1 : var2;
}

// Expected rewrite:
// base::span<int> fct3()
base::span<int> fct3() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  return var1++;
}

// Expected rewrite:
// base::span<int> fct3()
base::span<int> fct4() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  return ++var1;
}

// Expected rewrite:
// base::span<int> fct5()
base::span<int> fct5() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  return var1 + offset;
}

// Expected rewrite:
// base::span<char> fct6()
base::span<char> fct6() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // No rewrite here for now.
  return reinterpret_cast<char*>(var1 + offset);
}

// Function return type not rewritten since not used.
int* fct7() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return (var1 + offset).data();
  return (var1 + offset).data();
}

// Function return type not rewritten since not used.
char* fct8() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return (reinterpret_cast<char*>(var1) + offset).data();
  return (reinterpret_cast<char*>(var1) + offset).data();
}

void usage() {
  // Expected rewrite:
  // base::span<int> v1 = fct1();
  base::span<int> v1 = fct1();
  v1++;

  // Expected rewrite:
  // base::span<int> v2 = fct2();
  base::span<int> v2 = fct2();
  v2++;

  // Expected rewrite:
  // base::span<int> v3 = fct3();
  base::span<int> v3 = fct3();
  v3++;

  // Expected rewrite:
  // base::span<int> v4 = fct4();
  base::span<int> v4 = fct4();
  v4++;

  // Expected rewrite:
  // base::span<int> v5 = fct5();
  base::span<int> v5 = fct5();
  v5++;

  // Expected rewrite:
  // base::span<char> v6 = fct6();
  base::span<char> v6 = fct6();
  v6++;
}
