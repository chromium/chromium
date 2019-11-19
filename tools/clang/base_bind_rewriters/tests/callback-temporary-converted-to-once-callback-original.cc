// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "callback.h"

void Foo(base::OnceClosure) {}

void Test() {
  base::OnceClosure cb = base::Bind([] {});
  Foo(base::Bind([] {}));
  base::OnceClosure cb2 = base::BindRepeating([] {});
  Foo(base::BindRepeating([] {}));

  using namespace base;

  OnceClosure cb3 = Bind([] {});
  Foo(Bind([] {}));
  OnceClosure cb4 = BindRepeating([] {});
  Foo(BindRepeating([] {}));

  Closure cb5 = base::Bind([] {});
  Closure cb6 = base::BindRepeating([] {});
  RepeatingClosure cb7 = base::Bind([] {});
  RepeatingClosure cb8 = base::BindRepeating([] {});
}
