// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "callback.h"

void Test() {
  using base::BindOnce;
  using base::Passed;
  int i = 0;
  int* p = nullptr;

  // Passed takes a pointer and the address is taken here.
  // Remove `&` and replace base::Passed with std::move.
  base::BindOnce([] {}, std::move(i));

  // Passed takes a pointer. Replace base::Passed with std::move plus deref.
  base::BindOnce([] {}, std::move(*p));

  // The parameter is already rvalue-reference. Just remove base::Passed.
  // Plus, check if unqualified names work.
  base::BindOnce([] {}, std::move(*p));
  BindOnce([] {}, 1);
}
