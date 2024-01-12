// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"

#include "puffin/src/bit_reader.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/puff_writer.h"

using puffin::BitExtent;
using puffin::Buffer;
using puffin::BufferBitReader;
using puffin::BufferPuffWriter;
using puffin::Puffer;
using std::vector;

namespace {
void FuzzPuff(const uint8_t* data, size_t size) {
  BufferBitReader bit_reader(data, size);
  Buffer puff_buffer(size * 2);
  BufferPuffWriter puff_writer(puff_buffer.data(), puff_buffer.size());
  vector<BitExtent> bit_extents;
  Puffer puffer;
  puffer.PuffDeflate(&bit_reader, &puff_writer, &bit_extents);
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

  FuzzPuff(data, size);
  return 0;
}
