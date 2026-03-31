// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/core/SkSpan.h"

void fct() {
  int buf[10];
  // Expected rewrite:
  // SkSpan<int> ptr = buf;
  SkSpan<int> ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}
