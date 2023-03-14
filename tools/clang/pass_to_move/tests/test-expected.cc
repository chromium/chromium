// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

struct A {
  A&& Pass();
};

struct B {
  B& Pass();
};

struct C {
  A a;
};

struct D {
  D&& NotPass();
};

struct E {
  E() : a(new A) {}
  ~E() { delete a; }
  A* a;
};

struct F {
  explicit F(A&&);
  F&& Pass();
};

void Test() {
  // Pass that returns rvalue reference should use std::move.
  A a1;
  A a2 = std::move(a1);

  // Pass that doesn't return a rvalue reference should not be rewritten.
  B b1;
  B b2 = b1.Pass();

  // std::move() needs to wrap the entire expression when passing a member.
  C c;
  A a3 = std::move(c.a);

  // Don't rewrite things that return rvalue references that aren't named Pass.
  D d1;
  D d2 = d1.NotPass();

  // Pass via a pointer type should dereference the pointer first.
  E e;
  A a4 = std::move(*e.a);

  // Nested Pass() is handled correctly.
  A a5;
  F f = std::move(F(std::move(a5)));

  // Chained Pass is handled correctly.
  A a6;
  A a7 = std::move(a6);
}
