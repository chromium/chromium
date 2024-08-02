// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"

struct S {};

// Expected rewrite:
// S fct_declaration(base::span<const S> arg1,int arg2,S* arg3);
S fct_declaration(base::span<const S> arg1, int arg2, S* arg3);

// Expected rewrite:
// int fct_declaration2(base::raw_span<int> arg);
int fct_declaration2(base::raw_span<int> arg);

class Parent {
 public:
  Parent() = default;
  // Expected rewrite:
  // Parent(base::span<S> ptr): member(ptr){}
  Parent(base::span<S> ptr) : member(ptr) {}

  // Expected rewrite:
  // virtual base::span<S> get();
  virtual base::span<S> get();

 protected:
  // Expected rewrite:
  // base::span<S> member;
  base::span<S> member;
};

// Expected rewrite:
// virtual base::span<S> Parent::get()
base::span<S> Parent::get() {
  // Expected rewrite:
  // fct_declaration(member, 0, member.data());
  fct_declaration(member, 0, member.data());
  return member;
}

class Child : public Parent {
 public:
  Child() = default;

  // Expected rewrite:
  // base::span<S> get() override;
  base::span<S> get() override;
};

// Expected rewrite:
// base::span<S> Child::get()
base::span<S> Child::get() {
  // Expected rewrite:
  // return {};
  return {};
}

// Expected rewrite:
// S fct_declaration(base::span<const S> arg1, int arg2, S* arg3)
S fct_declaration(base::span<const S> arg1, int arg2, S* arg3) {
  // leads arg1 to be rewritten.
  return arg1[1];
}

// Expected rewrite:
// int fct_declaration2(base::raw_span<int> arg)
int fct_declaration2(base::raw_span<int> arg) {
  return arg[1];
}

void fct() {
  // Expected rewrite:
  // Parent p({});
  Parent p({});
  // Leads Parent::get() return value to be rewritten.
  p.get()[0] = {};

  // Expected rewrite:
  // fct_declaration2({});
  fct_declaration2({});
}
