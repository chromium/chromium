// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstring>

// This test case mimics the scenario in Skia where a pointer is spanified,
// and an index/offset of type 'int' is used in pointer arithmetic.
// Since 'base::checked_cast' is not available in Skia, the tool should
// use 'SkTo<size_t>' for the safe conversion.

// Expected rewrite:
// void copy(void* dst, SkSpan<const uint8_t> src, int offset, int size) {
//   memcpy(dst, src.subspan(SkTo<size_t>(offset)).data(), size);
// }
void copy(void* dst, const uint8_t* src, int offset, int size) {
  memcpy(dst, src + offset, size);
}
