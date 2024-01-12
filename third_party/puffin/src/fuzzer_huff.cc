// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "puffin/src/bit_writer.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/puff_reader.h"

using puffin::Buffer;
using puffin::BufferBitWriter;
using puffin::BufferPuffReader;
using puffin::ByteExtent;
using puffin::Huffer;

namespace {
void FuzzHuff(const uint8_t* data, size_t size) {
  BufferPuffReader puff_reader(data, size);
  Buffer deflate_buffer(size);
  BufferBitWriter bit_writer(deflate_buffer.data(), deflate_buffer.size());
  Huffer huffer;
  huffer.HuffDeflate(&puff_reader, &bit_writer);
}

class Environment {
 public:
  Environment() {
    // To turn off the logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  FuzzHuff(data, size);
  return 0;
}
