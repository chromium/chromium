// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

struct S {};
// Expected rewrite: typedef std::vector<raw_ptr<S>> VECTOR;
typedef std::vector<S*> VECTOR;

// No rewrite expected as this is not reachable from any field.
typedef std::vector<S*> VECTOR2;

class A {
 public:
  // Expected rewrite: A(const std::vector<raw_ptr<S>>& arg)
  A(const std::vector<S*>& arg) : member(arg) {}

  VECTOR get() { return member; }

  void fct() {
    // Expected rewrite: for(S* i : member)
    for (auto* i : member) {
      (void)i;
    }
  }

 private:
  VECTOR member;
};

void iterate(const VECTOR2& arg) {
  // No rewrite expected.
  for (auto* i : arg) {
    (void)i;
  }
}

void iterate2(const VECTOR& arg) {
  // Expected rewrite: for(S* i : arg)
  for (auto* i : arg) {
    (void)i;
  }

  // Expected rewrite: for(const S* i : arg)
  for (const auto* i : arg) {
    (void)i;
  }

  // Expected rewrite: for(const S* const i : arg)
  for (const auto* const i : arg) {
    (void)i;
  }
}

void function() {
  // Expected rewrite: std::vector<raw_ptr<S>> temp;
  std::vector<S*> temp;
  A a(temp);

  auto temp2 = a.get();
  // Expected rewrite: for(S* i : temp2)
  for (auto* i : temp2) {
    (void)i;
  }

  // Expected rewrite: auto* var1 = temp2.front().get();
  auto* var1 = temp2.front();
  (void)var1;

  // Expected rewrite: auto* var2 = temp2.back().get();
  auto* var2 = temp2.back();
  (void)var2;

  // Expected rewrite: auto* var3 = temp2[0].get();
  auto* var3 = temp2[0];
  (void)var3;
}
