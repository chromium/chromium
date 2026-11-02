// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

int UnsafeIndex();

// Expected rewrite:
// void processIntBuffer(SkSpan<int> buf)
void processIntBuffer(int* buf) {
  buf[0] = 1;
}

// Expected rewrite:
// void processUint8Buffer(SkSpan<uint8_t> buf)
void processUint8Buffer(uint8_t* buf) {
  buf[0] = 1;
}

void testSingleVariableAndWritableByteSpan() {
  int singleInt = 0;
  // Expected rewrite:
  // processIntBuffer(SkSpanFromRef(singleInt));
  processIntBuffer(&singleInt);
  // Expected rewrite:
  // processUint8Buffer(SkAsWritableByteSpan(SkSpanFromRef(singleInt)));
  processUint8Buffer(reinterpret_cast<uint8_t*>(&singleInt));
}

uint8_t CastStringToUint8(const std::string& arg) {
  // Expected rewrite:
  // SkSpan<const uint8_t> offset = SkAsByteSpan(arg);
  const uint8_t* offset = reinterpret_cast<const uint8_t*>(arg.data());
  return offset[UnsafeIndex()];
}
