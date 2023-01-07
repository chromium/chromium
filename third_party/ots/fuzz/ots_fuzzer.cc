// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/ots/src/include/opentype-sanitiser.h"
#include "third_party/ots/src/include/ots-memory-stream.h"

static uint8_t buffer[256 * 1024] = { 0 };

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ots::OTSContext context;
  ots::MemoryStream stream(static_cast<void*>(buffer), sizeof(buffer));
  context.Process(&stream, data, size);
  return 0;
}
