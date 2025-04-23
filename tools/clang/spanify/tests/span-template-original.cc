// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

int UnsafeIndex();  // Return out of bounds index.

// Regression test. This shouldn't violate assertions.
// TODO(crbug.com/393402160): Need to ensure all instantiations have know sizes.
//
// Expected rewrite:
// template <typename T>
// void f(base::span<T> t) {
template <typename T>
void f(T* t) {
  t[UnsafeIndex()] = 0;
}

void test_with_template() {
  int a[10];
  f(a);
}
