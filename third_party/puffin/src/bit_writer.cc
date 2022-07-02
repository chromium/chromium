// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/bit_writer.h"

#include <algorithm>

#include "puffin/src/logging.h"

namespace puffin {

bool BufferBitWriter::WriteBits(size_t nbits, uint32_t bits) {
  TEST_AND_RETURN_FALSE(((out_size_ - index_) * 8) - out_holder_bits_ >= nbits);
  TEST_AND_RETURN_FALSE(nbits <= sizeof(bits) * 8);
  while (nbits > 0) {
    while (out_holder_bits_ >= 8) {
      out_buf_[index_++] = out_holder_ & 0x000000FF;
      out_holder_ >>= 8;
      out_holder_bits_ -= 8;
    }
    while (out_holder_bits_ < 24 && nbits > 0) {
      out_holder_ |= (bits & 0x000000FF) << out_holder_bits_;
      auto min = std::min(nbits, static_cast<size_t>(8));
      out_holder_bits_ += min;
      bits >>= min;
      nbits -= min;
    }
  }
  return true;
}

bool BufferBitWriter::WriteBytes(
    size_t nbytes,
    const std::function<bool(uint8_t* buffer, size_t count)>& read_fn) {
  TEST_AND_RETURN_FALSE(((out_size_ - index_) * 8) - out_holder_bits_ >=
                        (nbytes * 8));
  TEST_AND_RETURN_FALSE(out_holder_bits_ % 8 == 0);
  TEST_AND_RETURN_FALSE(Flush());
  TEST_AND_RETURN_FALSE(read_fn(&out_buf_[index_], nbytes));
  index_ += nbytes;
  return true;
}

bool BufferBitWriter::WriteBoundaryBits(uint8_t bits) {
  return WriteBits((8 - (out_holder_bits_ & 7)) & 7, bits);
}

bool BufferBitWriter::Flush() {
  TEST_AND_RETURN_FALSE(WriteBoundaryBits(0));
  while (out_holder_bits_ > 0) {
    out_buf_[index_++] = out_holder_ & 0x000000FF;
    out_holder_ >>= 8;
    out_holder_bits_ -= 8;
  }
  return true;
}

size_t BufferBitWriter::Size() const {
  return index_;
}

}  // namespace puffin
