// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The `++` is in a macro body, so it can't be rewritten in place. `ptr` is
// excluded and stays a pointer.
#define PRE_INC(x) ++x

int* test_macro_increment(int buf[3]) {
  int* ptr = buf;
  // Regression test for "++i" crashing when written inside macros.
  PRE_INC(ptr);
  return ptr;
}

int main() {
  int buf[3] = {1, 2, 3};
  int* ptr = test_macro_increment(buf);
  return ptr[0];
}
