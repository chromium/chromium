// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

void fct() {
  int buf[10];
  // Expected rewrite:
  // base::span<int> ptr = buf;
  base::span<int> ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}
