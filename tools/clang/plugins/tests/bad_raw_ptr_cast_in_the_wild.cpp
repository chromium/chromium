// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

// Real examples of bad casting, cited from here.
// https://docs.google.com/document/d/14Ol_adOdNpy4Ge-XReI7CXNKMzs_LL5vucDQIERDQyg/edit?usp=sharing

// ==============================
// Example 1. "Initialization"
// ==============================
struct A {
  raw_ptr<int> ptr;
};

A* ExampleOne(void* buf) {
  return reinterpret_cast<A*>(buf);  // Should error.
}

// ==============================
// Example 2. "Matching Struct"
// ==============================
struct ThirdPartyA {
  int* ptr;
};

A* ExampleTwo(ThirdPartyA* obj) {
  return reinterpret_cast<A*>(obj);  // Should error.
}

// ==============================
// Example 3. "Reinterpreting as void**"
// ==============================
int** ExampleThree(raw_ptr<int>* ptr) {
  return reinterpret_cast<int**>(ptr);  // Should error.
}

// ==============================
// Example 4. "Reinterpreting pointer to embedder class as void*"
// ==============================
void* my_memset(void* s, int c, int n);

void ExampleFour() {
  A obj;
  A* obj_ptr = &obj;
  my_memset(obj_ptr, 0, sizeof(obj_ptr));  // Should error.
}
