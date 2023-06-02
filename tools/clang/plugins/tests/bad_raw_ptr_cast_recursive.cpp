// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

struct A;
struct B;
struct C;
struct D;

// raw_ptr<int> <-- A <-> B --> C
//                        ^
//                        |
//                        D
// A, B and D are casting-unsafe.
struct A {
  B* b;
  raw_ptr<int> ptr;
};

struct B {
  A* a;
  C* c;
};

struct C {};

struct D {
  B* b;
};

void CastToCastingUnsafe() {
  void* p = nullptr;

  (void)static_cast<A*>(p);  // Error.
  (void)static_cast<B*>(p);  // Error.
  (void)static_cast<C*>(p);  // OK.
  (void)static_cast<D*>(p);  // Error.
}
