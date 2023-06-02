// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/puff_reader.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "puffin/src/logging.h"

namespace puffin {

namespace {
// Reads a value from the buffer in big-endian mode.
inline uint16_t ReadByteArrayToUint16(const uint8_t* buffer) {
  return (*buffer << 8) | *(buffer + 1);
}
}  // namespace

bool BufferPuffReader::GetNext(PuffData* data) {
  PuffData& pd = *data;
  size_t length = 0;
  if (state_ == State::kReadingLenDist) {
    // Boundary check
    TEST_AND_RETURN_FALSE(index_ < puff_size_);
    if (puff_buf_in_[index_] & 0x80) {  // Reading length/distance.
      if ((puff_buf_in_[index_] & 0x7F) < 127) {
        length = puff_buf_in_[index_] & 0x7F;
      } else {
        index_++;
        // Boundary check
        TEST_AND_RETURN_FALSE(index_ < puff_size_);
        length = puff_buf_in_[index_] + 127;
      }
      length += 3;
      TEST_AND_RETURN_FALSE(length <= 259);

      index_++;

      // End of block. End of block is similar to length/distance but without
      // distance value and length value set to 259.
      if (length == 259) {
        pd.type = PuffData::Type::kEndOfBlock;
        state_ = State::kReadingBlockMetadata;
        return true;
      }

      // Boundary check
      TEST_AND_RETURN_FALSE(index_ + 1 < puff_size_);
      auto distance = ReadByteArrayToUint16(&puff_buf_in_[index_]);
      // The distance in RFC is in the range [1..32768], but in the puff spec,
      // we write zero-based distance in the puff stream.
      TEST_AND_RETURN_FALSE(distance < (1 << 15));
      distance++;
      index_ += 2;

      pd.type = PuffData::Type::kLenDist;
      pd.length = length;
      pd.distance = distance;
      return true;
    } else {  // Reading literals.
      // Boundary check
      TEST_AND_RETURN_FALSE(index_ < puff_size_);
      if ((puff_buf_in_[index_] & 0x7F) < 127) {
        length = puff_buf_in_[index_] & 0x7F;
        index_++;
      } else {
        index_++;
        // Boundary check
        TEST_AND_RETURN_FALSE(index_ + 1 < puff_size_);
        length = ReadByteArrayToUint16(&puff_buf_in_[index_]) + 127;
        index_ += 2;
      }
      length++;
      // Boundary check
      TEST_AND_RETURN_FALSE(index_ + length <= puff_size_);
      pd.type = PuffData::Type::kLiterals;
      pd.length = length;
      pd.read_fn = [this, length](uint8_t* buffer, size_t count) mutable {
        TEST_AND_RETURN_FALSE(count <= length);
        memcpy(buffer, &puff_buf_in_[index_], count);
        index_ += count;
        length -= count;
        return true;
      };
      return true;
    }
  } else {  // Block metadata
    pd.type = PuffData::Type::kBlockMetadata;
    // Boundary check
    TEST_AND_RETURN_FALSE(index_ + 2 < puff_size_);
    length = ReadByteArrayToUint16(&puff_buf_in_[index_]) + 1;
    index_ += 2;
    // Boundary check
    TEST_AND_RETURN_FALSE(index_ + length <= puff_size_);
    TEST_AND_RETURN_FALSE(length <= sizeof(pd.block_metadata));
    memcpy(pd.block_metadata, &puff_buf_in_[index_], length);
    index_ += length;
    pd.length = length;
    state_ = State::kReadingLenDist;
  }
  return true;
}

size_t BufferPuffReader::BytesLeft() const {
  return puff_size_ - index_;
}

}  // namespace puffin
