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
  std::vector<int> ctn1 = {1, 2, 3, 4};
  std::array<int, 3> ctn2 = {5, 6, 7};

  // Expected rewrite:
  // base::span<int> aa = {};
  int* aa = nullptr;
  // Expected rewrite:
  // base::span<int> bb = {};
  int* bb = nullptr;
  // Expected rewrite:
  // base::span<int> cc = {};
  int* cc = nullptr;

  // Expected rewrite:
  // aa = ctn1;
  aa = ctn1.data();
  // Expected rewrite:
  // bb = ctn2;
  bb = ctn2.data();

  bb = aa;
  cc = bb;

  cc[index] = value;  // Buffer usage, leads c to be rewritten.

  // Expected rewrite:
  // base::raw_span<int> dd = {};
  raw_ptr<int> dd = nullptr;

  dd = cc;

  dd[index] = value;  // Buffer usage, leads d to be rewritten.

  // Expected rewrite:
  // base::span<int> ee = {};
  int* ee = nullptr;

  // Expected rewrite:
  // ee = dd;
  ee = dd.get();

  ee++;  // Buffer usage, leads e to be rewritten.

  // Expected rewrite:
  // base::span<int> ff = {};
  int* ff = nullptr;

  ff = get<int>();

  ++ff;  // Leads to ff being rewritten.

  // Exptected rewrite:
  // base::span<int> gg = {};
  int* gg = nullptr;
  bool condition = true;
  // Expected rewrite:
  // gg = (condition) ? ctn1 : ctn2;
  gg = (condition) ? ctn1.data() : ctn2.data();

  gg += 1;  // Buffer usage, leads gg to be rewritten.
}
