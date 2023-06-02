// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/bit_reader.h"

#include <cstring>

#include "puffin/src/logging.h"

namespace puffin {

bool BufferBitReader::CacheBits(size_t nbits) {
  if ((in_size_ - index_) * 8 + in_cache_bits_ < nbits) {
    return false;
  }
  if (nbits > sizeof(in_cache_) * 8) {
    return false;
  }
  while (in_cache_bits_ < nbits) {
    in_cache_ |= in_buf_[index_++] << in_cache_bits_;
    in_cache_bits_ += 8;
  }
  return true;
}

uint32_t BufferBitReader::ReadBits(size_t nbits) {
  return in_cache_ & ((1U << nbits) - 1);
}

void BufferBitReader::DropBits(size_t nbits) {
  in_cache_ >>= nbits;
  in_cache_bits_ -= nbits;
}

uint8_t BufferBitReader::ReadBoundaryBits() {
  return in_cache_ & ((1 << (in_cache_bits_ & 7)) - 1);
}

size_t BufferBitReader::SkipBoundaryBits() {
  size_t nbits = in_cache_bits_ & 7;
  in_cache_ >>= nbits;
  in_cache_bits_ -= nbits;
  return nbits;
}

bool BufferBitReader::GetByteReaderFn(
    size_t length, std::function<bool(uint8_t*, size_t)>* read_fn) {
  index_ -= (in_cache_bits_ + 7) / 8;
  in_cache_ = 0;
  in_cache_bits_ = 0;
  TEST_AND_RETURN_FALSE(length <= in_size_ - index_);
  *read_fn = [this, length](uint8_t* buffer, size_t count) mutable {
    TEST_AND_RETURN_FALSE(count <= length);
    if (buffer != nullptr) {
      memcpy(buffer, &in_buf_[index_], count);
    }
    index_ += count;
    length -= count;
    return true;
  };
  return true;
}

size_t BufferBitReader::Offset() const {
  return index_ - in_cache_bits_ / 8;
}

uint64_t BufferBitReader::OffsetInBits() const {
  return (index_ * 8) - in_cache_bits_;
}

uint64_t BufferBitReader::BitsRemaining() const {
  return ((in_size_ - index_) * 8) + in_cache_bits_;
}

}  // namespace puffin
