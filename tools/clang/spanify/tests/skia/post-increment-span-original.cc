// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test case mimics the scenario in Skia where a pointer is spanified,
// and post-increment operator is used on it.
// The increments are rewritten using only the span's own `subspan()` and the
// standard library, so no project-specific helper is needed.

// Expected rewrite:
// void copy(SkSpan<int> dst, SkSpan<int> src, int count) {
//   while (count-- > 0) {
//     std::exchange(dst, dst.subspan(1u))[0] = (src = src.subspan(1u))[0];
//   }
// }
void copy(int* dst, int* src, int count) {
  while (count-- > 0) {
    *dst++ = *++src;
  }
}
