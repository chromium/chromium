// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace gfx {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // TODO(crbug.com/323934468): Initialize decoder settings dynamically using
  // fuzzer input.
  JPEGCodec::Decode(data, size);
  return 0;
}

}  // namespace gfx
