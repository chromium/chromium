// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"

// Regression tests about `base::span` not supporting operator bool()
// This is based on `WaitMany` from mojo/public/cpp/system/wait.cc.

using Handle = int;

// Expected rewrite:
// void f(const base::span<Handle>& handles, size_t num_handles) {
void f(Handle* handles, size_t num_handles) {
  // Expected rewrite:
  // if (handles.empty() || !num_handles)
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
  f(nullptr, 0);
  f(nullptr, 2);
  f(handles, 0);
}

// Regression tests to ensure `!` won't be added into a macro definition.
// `!` should be added before the macro argument.

// The following code pattern is actually used in CHECK macro family.
// No rewrite expected.
#define IMPLICITLY_CONVERT_TO_BOOL(expr) \
  if ((expr) ? true : false) {           \
  }

void implicit_cast_to_bool() {
  // Expected rewrite:
  // base::span<int> buf = {};
  int* buf = nullptr;
  std::ignore = buf[0];

  // Expected rewrite:
  // IMPLICITLY_CONVERT_TO_BOOL(!buf.empty());
  IMPLICITLY_CONVERT_TO_BOOL(buf);
}

void implicit_call_to_operator_bool() {
  // Expected rewrite:
  // base::raw_span<int> buf = {};
  raw_ptr<int> buf = nullptr;
  // Expected rewrite:
  // base::span<int> ptr = buf;
  int* ptr = buf.get();
  std::ignore = ptr[0];

  // Expected rewrite:
  // IMPLICITLY_CONVERT_TO_BOOL(!buf.empty());
  IMPLICITLY_CONVERT_TO_BOOL(buf);
  // Expected rewrite:
  // IMPLICITLY_CONVERT_TO_BOOL(!ptr.empty());
  IMPLICITLY_CONVERT_TO_BOOL(ptr);
}
