// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "third_party/do_not_rewrite/third_party_api.h"

int UnsafeIndex();

struct S {};

// Expected rewrite:
// S fct_declaration(base::span<const S> arg1,int arg2,S* arg3);
S fct_declaration(const S* arg1, int arg2, S* arg3);

// Expected rewrite:
// int fct_declaration2(base::raw_span<int> arg);
int fct_declaration2(raw_ptr<int> arg);

class Parent {
 public:
  Parent() = default;
  // Expected rewrite:
  // Parent(base::span<S> ptr): member(ptr){}
  Parent(S* ptr) : member(ptr) {}

  // Expected rewrite:
  // virtual base::span<S> get();
  virtual S* get();

  // Expected rewrite:
  // virtual void Method1(base::span<S, 1 + 2> s);
  virtual void Method1(S s[1 + 2]);

 protected:
  // Expected rewrite:
  // base::span<S> member;
  S* member;
};

// Expected rewrite:
// virtual base::span<S> Parent::get()
S* Parent::get() {
  // Expected rewrite:
  // fct_declaration(member, 0, member.data());
  fct_declaration(member, 0, member);
  return member;
}

// Expected rewrite:
// void Parent::Method1(base::span<S, 2 + 1> s) {}
void Parent::Method1(S s[2 + 1]) {}

class Child : public Parent {
 public:
  Child() = default;

  // Expected rewrite:
  // base::span<S> get() override;
  S* get() override;

  // Expected rewrite:
  // void Method1(base::span<S, 0 + 3> s) override;
  void Method1(S s[0 + 3]) override;
};

// Expected rewrite:
// base::span<S> Child::get()
S* Child::get() {
  // Expected rewrite:
  // return {};
  return nullptr;
}

// Expected rewrite:
// void Child::Method1(base::span<S, 3 + 0> s) {
void Child::Method1(S s[3 + 0]) {
  member[0] = s[UnsafeIndex()];
}

// Expected rewrite:
// S fct_declaration(base::span<const S> arg1, int arg2, S* arg3)
S fct_declaration(const S* arg1, int arg2, S* arg3) {
  // leads arg1 to be rewritten.
  return arg1[1];
}

// Expected rewrite:
// int fct_declaration2(base::raw_span<int> arg)
int fct_declaration2(raw_ptr<int> arg) {
  return arg[1];
}

// Expected rewrite:
// void ManyDecls(base::span<S, 3> arg);
void ManyDecls(S arg[3]);
// Expected rewrite:
// void ManyDecls(base::span<S, 3> arg);
void ManyDecls(S arg[3]);
// Expected rewrite:
// void ManyDecls(base::span<S, 3> arg) {
void ManyDecls(S arg[3]) {
  arg[UnsafeIndex()] = S();
}
// Expected rewrite:
// void ManyDecls(base::span<S, 3> arg);
void ManyDecls(S arg[3]);

// Third-party code is not expected to be rewritten, hence this implementation
// class is also not expected to be rewritten.
class ChromiumImpl : public ThirdPartyInterface {
 public:
  void ToBeImplemented(int arg[3]) override;
};

void ChromiumImpl::ToBeImplemented(int arg[3]) {
  arg[UnsafeIndex()] = 0;
}

void fct() {
  // Expected rewrite:
  // Parent p({});
  Parent p(nullptr);
  // Leads Parent::get() return value to be rewritten.
  p.get()[0] = {};

  // Expected rewrite:
  // fct_declaration2({});
  fct_declaration2(nullptr);

  S s[3];
  ManyDecls(s);

  ChromiumImpl impl;
  // Expected rewrite:
  // std::array<int, 3> array = {1, 2, 3};
  int array[3]{1, 2, 3};
  // Expected rewrite:
  // impl.ToBeImplemented(array.data());
  impl.ToBeImplemented(array);
  array[UnsafeIndex()] = 0;
}
