// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cstdint>
#include <vector>

#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

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
  // return base::PostIncrementSpan(var1);
  return base::PostIncrementSpan(var1);
}

// Expected rewrite:
// base::span<int> fct3()
base::span<int> fct4() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  // return base::PreIncrementSpan(var1);
  return base::PreIncrementSpan(var1);
}

// Expected rewrite:
// base::span<int> fct5()
base::span<int> fct5() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return var1.subspan(base::checked_cast<size_t>(offset));
  return var1.subspan(base::checked_cast<size_t>(offset));
}

// Expected rewrite:
// base::span<char> fct6()
base::span<char> fct6() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return
  // base::as_writable_byte_span(var1.subspan(base::checked_cast<size_t>(offset)));
  return base::as_writable_byte_span(
      var1.subspan(base::checked_cast<size_t>(offset)));
}

// Function return type not rewritten since not used.
int* fct7() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return var1.subspan(base::checked_cast<size_t>(offset)).data();
  return var1.subspan(base::checked_cast<size_t>(offset)).data();
}

// Function return type not rewritten since not used.
char* fct8() {
  bool condition = true;
  // Expected rewrite:
  // base::span<int> var1 = new int[1024];
  base::span<int> var1 = new int[1024];
  int offset = 1;
  // Expected rewrite:
  // return
  // reinterpret_cast<char*>(var1).subspan(base::checked_cast<size_t>(offset)).data();
  // As-is, this code doesn't compile because we don't yet handle
  // adapting these reinterpret_cast expressions for spans.
  return reinterpret_cast<char*>(var1)
      .subspan(base::checked_cast<size_t>(offset))
      .data();
}

void usage() {
  // Expected rewrite:
  // base::span<int> v1 = fct1();
  base::span<int> v1 = fct1();
  // base::PostIncrementSpan(v1);
  base::PostIncrementSpan(v1);

  // Expected rewrite:
  // base::span<int> v2 = fct2();
  base::span<int> v2 = fct2();
  // base::PostIncrementSpan(v2);
  base::PostIncrementSpan(v2);

  // Expected rewrite:
  // base::span<int> v3 = fct3();
  base::span<int> v3 = fct3();
  // base::PostIncrementSpan(v3);
  base::PostIncrementSpan(v3);

  // Expected rewrite:
  // base::span<int> v4 = fct4();
  base::span<int> v4 = fct4();
  // base::PostIncrementSpan(v4);
  base::PostIncrementSpan(v4);

  // Expected rewrite:
  // base::span<int> v5 = fct5();
  base::span<int> v5 = fct5();
  // base::PostIncrementSpan(v5);
  base::PostIncrementSpan(v5);

  // Expected rewrite:
  // base::span<char> v6 = fct6();
  base::span<char> v6 = fct6();
  // base::PostIncrementSpan(v6);
  base::PostIncrementSpan(v6);
}
