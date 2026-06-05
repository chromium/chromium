// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/core/SkSpan.h"

template <typename T>
void init9(SkSpan<T> array) {
  for (int i = 0; i < 9; i++) {
    array[i] = T();
  }
}
