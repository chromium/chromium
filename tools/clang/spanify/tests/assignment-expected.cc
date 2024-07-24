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
  std::vector<int> ctn1 = {1, 2, 3, 4};
  std::array<int, 3> ctn2 = {5, 6, 7};

  // Expected rewrite:
  // base::span<int> aa = {};
  base::span<int> aa = {};
  // Expected rewrite:
  // base::span<int> bb = {};
  base::span<int> bb = {};
  // Expected rewrite:
  // base::span<int> cc = {};
  base::span<int> cc = {};

  // Expected rewrite:
  // aa = ctn1;
  aa = ctn1;
  // Expected rewrite:
  // bb = ctn2;
  bb = ctn2;

  bb = aa;
  cc = bb;

  cc[index] = value;  // Buffer usage, leads c to be rewritten.

  // Expected rewrite:
  // base::raw_span<int> dd = {};
  base::raw_span<int> dd = {};

  dd = cc;

  dd[index] = value;  // Buffer usage, leads d to be rewritten.

  // Expected rewrite:
  // base::span<int> ee = {};
  base::span<int> ee = {};

  // Expected rewrite:
  // ee = dd;
  ee = dd;

  ee++;  // Buffer usage, leads e to be rewritten.

  // Expected rewrite:
  // base::span<int> ff = {};
  base::span<int> ff = {};

  ff = get<int>();

  ++ff;  // Leads to ff being rewritten.

  // Exptected rewrite:
  // base::span<int> gg = {};
  base::span<int> gg = {};
  bool condition = true;
  // Expected rewrite:
  // gg = (condition) ? ctn1 : ctn2;
  gg = (condition) ? ctn1 : ctn2;

  gg += 1;  // Buffer usage, leads gg to be rewritten.
}
