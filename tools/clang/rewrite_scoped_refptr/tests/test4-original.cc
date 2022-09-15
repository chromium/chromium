// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_refptr.h"

struct Foo {
  int dummy;
};

void ExpectsRawPtr(Foo* foo) {
  Foo* temp = foo;
}

void CallExpectsRawPtrWithScopedRefptr() {
  scoped_refptr<Foo> ok(new Foo);
  ExpectsRawPtr(ok);
}

void CallExpectsRawPtrWithRawPtr() {
  ExpectsRawPtr(new Foo);
}
