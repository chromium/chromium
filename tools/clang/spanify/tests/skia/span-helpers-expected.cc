// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "include/core/SkSpan.h"

int UnsafeIndex();

// Expected rewrite:
// void processIntBuffer(SkSpan<int> buf)
void processIntBuffer(SkSpan<int> buf) {
  buf[0] = 1;
}

// Expected rewrite:
// void processUint8Buffer(SkSpan<uint8_t> buf)
void processUint8Buffer(SkSpan<uint8_t> buf) {
  buf[0] = 1;
}

void testSingleVariableAndWritableByteSpan() {
  int singleInt = 0;
  // Expected rewrite:
  // processIntBuffer(SkSpanFromRef(singleInt));
  processIntBuffer(SkSpanFromRef(singleInt));
  // Expected rewrite:
  // processUint8Buffer(SkAsWritableByteSpan(SkSpanFromRef(singleInt)));
  processUint8Buffer(SkAsWritableByteSpan(SkSpanFromRef(singleInt)));
}

uint8_t CastStringToUint8(const std::string& arg) {
  // Expected rewrite:
  // SkSpan<const uint8_t> offset = SkAsByteSpan(arg);
  SkSpan<const uint8_t> offset = SkAsByteSpan(arg);
  return offset[UnsafeIndex()];
}
