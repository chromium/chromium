// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test case mimics the scenario in Skia where a pointer is spanified,
// and post-increment operator is used on it.
// Since 'base::PostIncrementSpan' is not available in Skia, the tool should
// use 'SkPostIncrementSpan'

#include "include/core/SkSpan.h"

// Expected rewrite:
// void copy(SkSpan<int> dst, SkSpan<int> src, int count) {
//   while (count-- > 0) {
//     (SkPostIncrementSpan(dst))[0] = (SkPreIncrementSpan(src))[0];
//   }
// }
void copy(SkSpan<int> dst, SkSpan<int> src, int count) {
  while (count-- > 0) {
    (SkPostIncrementSpan(dst))[0] = (SkPreIncrementSpan(src))[0];
  }
}
