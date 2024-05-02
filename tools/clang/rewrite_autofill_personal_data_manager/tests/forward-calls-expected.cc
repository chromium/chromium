// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "some/file.h"

class Foo {
 public:
  void bar(int x) {}
  void not_bar() {}
  Foo& baz() { return *this; }
};

class FooDerived : public Foo {};

class NotFoo {
 public:
  void bar() {}
};

Foo& get_foo() {
  static Foo f;
  return f;
}

// Tests that only `bar()` calls on `Foo`s are rewritten.
void RewritesFoo() {
  Foo foo;
  foo.baz().bar(1);
  foo.not_bar();
}

// Tests that `bar()` calls on `Foo` pointers are rewritten.
void RewritesFooPtr() {
  Foo foo;
  Foo* foo_ptr = &foo;
  foo_ptr->baz().bar(3);
}

// Tests that `bar()` calls on `Foo` referencess are rewritten.
void RewritesFooRef() {
  Foo foo;
  Foo& foo_ref = foo;
  foo_ref.baz().bar(4);
}

// Tests that when the `Foo` is retried through a function call, rewrites work.
void RewritesFooIndirection() {
  get_foo().baz().bar(5);
}

// Tests that `bar()` calls on classes derived from `Foo` are rewritten.
void RewritesFooDerived() {
  FooDerived foo;
  foo.baz().bar(6);
}

// Tests that `bar()` calls on non-`Foo` classes are not rewritten.
void DoesntRewriteNotFoo() {
  NotFoo not_foo;
  not_foo.bar();
}
