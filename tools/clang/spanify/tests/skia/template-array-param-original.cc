// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

template <typename T>
void init9(T array[9]) {
  for (int i = 0; i < 9; i++) {
    array[i] = T();
  }
}
