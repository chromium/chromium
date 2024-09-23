// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

// Regression tests about `base::span` not supporting operator bool()
// This is based on `WaitMany` from mojo/public/cpp/system/wait.cc.

#include "base/containers/span.h"

using Handle = int;

// Expected rewrite:
// void f(const base::span<Handle>& handles, size_t num_handles) {
void f(base::span<Handle> handles, size_t num_handles) {
  // TODO(358306232) operator bool() is not supported by base::span.
  if (!handles || !num_handles) {
    return;
  }

  for (size_t i = 0; i < num_handles; ++i) {
    // Do something with handles[i]
    handles[i]++;
  }
}

void g() {
  Handle handles[2] = {1, 2};
  f(handles, 2);
  f({}, 0);
  f({}, 2);
  f(handles, 0);
}
