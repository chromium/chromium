// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

struct S {};

// Expected rewrite:(const std::vector<raw_ptr<S>>& arg1,
//                   int arg2,
//                   std::vector<raw_ptr<S>>* arg3);
void fct_declaration(const std::vector<S*>& arg1,
                     int arg2,
                     std::vector<S*>* arg3);

class Parent {
 public:
  Parent() = default;

  // Expected rewrite: virtual std::vector<raw_ptr<S>> get();
  virtual std::vector<S*> get();

 protected:
  // Expected rewrite: std::vector<raw_ptr<S>> member;
  std::vector<S*> member;
};

// Expected rewrite: virtual std::vector<raw_ptr<S>> Parent::get()
std::vector<S*> Parent::get() {
  fct_declaration(member, 0, &member);
  return member;
}

class Child : public Parent {
 public:
  Child() = default;

  // Expected rewrite: virtual std::vector<raw_ptr<S>> get();
  std::vector<S*> get() override;
};

// Expected rewrite: virtual std::vector<raw_ptr<S>> get();
std::vector<S*> Child::get() {
  // Expected rewrite: return std::vector<raw_ptr<S>>{};
  return std::vector<S*>{};
}

// Expected rewrite:(const std::vector<raw_ptr<S>>& arg1,
//                   int arg2,
//                   std::vector<raw_ptr<S>>* arg3);
void fct_declaration(const std::vector<S*>& arg1,
                     int arg2,
                     std::vector<S*>* arg3) {
  *arg3 = arg1;
}
