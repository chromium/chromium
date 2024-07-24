// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

class A {
 public:
  // Expected rewrite:
  // A(base::span<int> ptr): member(ptr){}
  A(base::span<int> ptr) : member(ptr) {}

  // Expecte rewrite:
  // int* advanceAndGet() { return member++.data(); }
  int* advanceAndGet() { return member++.data(); }

  // Expected rewrite:
  // int* get() { return member.data(); }
  int* get() { return member.data(); }

 private:
  // Expecte rewrite:
  // base::raw_span<int> member;
  base::raw_span<int> member;
};

struct B {
  // Expected rewrite:
  // base::span<int> member;
  base::span<int> member;
  std::size_t size;
};

void fct() {
  // Expected rewrite:
  // base::span<int> buf = new int[1024];
  base::span<int> buf = new int[1024];

  bool condition = true;
  std::vector<int> buf2 = {1, 2, 3};
  // Expected rewrite:
  // A obj((condition) ? buf : buf2);
  A obj((condition) ? buf : buf2);

  std::vector<int> buf3 = {4, 5, 6};
  // Expected rewrite:
  // B obj2{(condition) ? buf3 : buf2, buf3.size()};
  B obj2{(condition) ? buf3 : buf2, 2};
  // Leads member to be marked as a buffer.
  obj2.member[1] = 3;
}
