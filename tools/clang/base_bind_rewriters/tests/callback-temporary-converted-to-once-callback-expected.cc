// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "callback.h"

void Foo(base::OnceClosure) {}

void Test() {
  base::OnceClosure cb = base::BindOnce([] {});
  Foo(base::BindOnce([] {}));
  base::OnceClosure cb2 = base::BindOnce([] {});
  Foo(base::BindOnce([] {}));

  using namespace base;

  OnceClosure cb3 = BindOnce([] {});
  Foo(BindOnce([] {}));
  OnceClosure cb4 = BindOnce([] {});
  Foo(BindOnce([] {}));

  Closure cb5 = base::Bind([] {});
  Closure cb6 = base::BindRepeating([] {});
  RepeatingClosure cb7 = base::Bind([] {});
  RepeatingClosure cb8 = base::BindRepeating([] {});
}
