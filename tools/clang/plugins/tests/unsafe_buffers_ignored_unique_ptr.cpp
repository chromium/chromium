// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

template <typename T>
void unsafe_func(std::unique_ptr<T>& u) {
  u[5];
}

void foo(std::unique_ptr<int[]>& u) {
  unsafe_func(u);
}
