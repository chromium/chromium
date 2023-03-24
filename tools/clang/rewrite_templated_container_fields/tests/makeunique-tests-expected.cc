// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"

struct S {};

// Expected rewrite: std::vector<raw_ptr<S>> get();
std::vector<raw_ptr<S>> get() {
  return {};
}

// Expected rewrite: std::vector<raw_ptr<S>>& get2();
std::vector<raw_ptr<S>>& get2() {
  // Expected rewrite: static std::vector<raw_ptr<S>> v;
  static std::vector<raw_ptr<S>> v;
  return v;
}

class A {
 public:
  // Expected rewrite:  A(const std::vector<raw_ptr<S>>& arg1,
  A(const std::vector<raw_ptr<S>>& arg1,
    // Expected rewrite:  const std::vector<raw_ptr<S>>& arg2,
    const std::vector<raw_ptr<S>>& arg2,
    // Expected rewrite:  const std::vector<raw_ptr<S>>& arg3,
    const std::vector<raw_ptr<S>>& arg3,
    // No rewrite expected.
    const std::vector<S*>& arg4)
      : member1(arg1),
        member2(&arg2),
        member3(arg3.begin()),
        member4(get()),
        member5(&get2()),
        size_(arg4.size()) {}

 private:
  // Expected rewrite: std::vector<raw_ptr<S>> member1;
  std::vector<raw_ptr<S>> member1;
  // Expected rewrite: const std::vector<raw_ptr<S>>* member2;
  const std::vector<raw_ptr<S>>* member2;
  // Expected rewrite: std::vector<raw_ptr<S>>::iterator member3;
  std::vector<raw_ptr<S>>::const_iterator member3;
  // Expected rewrite: std::vector<raw_ptr<S>> member4;
  std::vector<raw_ptr<S>> member4;
  // Expected rewrite: std::vector<raw_ptr<S>>* member5;
  std::vector<raw_ptr<S>>* member5;
  std::size_t size_;
};

void fct() {
  // Expected rewrite: std::vector<raw_ptr<S>> temp1;
  std::vector<raw_ptr<S>> temp1;
  // Expected rewrite: std::vector<raw_ptr<S>> temp2;
  std::vector<raw_ptr<S>> temp2;
  // Expected rewrite: std::vector<raw_ptr<S>> temp3;
  std::vector<raw_ptr<S>> temp3;
  // No rewrite expected.
  std::vector<S*> temp4;

  auto uptr = std::make_unique<A>(temp1, temp2, temp3, temp4);
}

namespace n1 {
class B {
 public:
  // Expected rewrite:  A(const std::vector<raw_ptr<S>>& arg)
  B(const std::vector<raw_ptr<S>>& arg) : member(arg) {}

 private:
  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<raw_ptr<S>> member;
};

// Expected rewrite: std::vector<raw_ptr<S>> get_fct()
std::vector<raw_ptr<S>> get_fct() {
  return {};
}

// Expected rewrite: std::vector<raw_ptr<S>>* get_ptr_fct()
std::vector<raw_ptr<S>>* get_ptr_fct() {
  return {};
}

void fct() {
  auto uptr = std::make_unique<B>(get_fct());
  auto uptr2 = std::make_unique<B>(*get_ptr_fct());
  auto uptr3 = std::make_unique<B>(std::vector<raw_ptr<S>>());
}

// Expected rewrite: fct2(const std::vector<raw_ptr<S>>& arg)
void fct2(const std::vector<raw_ptr<S>>& arg) {
  auto uptr = std::make_unique<B>(arg);
}
}  // namespace n1
