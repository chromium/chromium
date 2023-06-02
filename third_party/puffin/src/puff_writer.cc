// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/puff_writer.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "puffin/src/logging.h"

namespace puffin {

namespace {
// Writes a value to the buffer in big-endian mode. Experience showed that
// big-endian creates smaller payloads.
inline void WriteUint16ToByteArray(uint16_t value, uint8_t* buffer) {
  *buffer = value >> 8;
  *(buffer + 1) = value & 0x00FF;
}

constexpr size_t kLiteralsMaxLength = (1 << 16) + 127;  // 65663
}  // namespace

bool BufferPuffWriter::Insert(const PuffData& pd) {
  switch (pd.type) {
    case PuffData::Type::kLiterals:
      if (pd.length == 0) {
        return true;
      }
      FALLTHROUGH_INTENDED;
    case PuffData::Type::kLiteral: {
      size_t length = pd.type == PuffData::Type::kLiteral ? 1 : pd.length;
      if (state_ == State::kWritingNonLiteral) {
        len_index_ = index_;
        index_++;
        state_ = State::kWritingSmallLiteral;
      }
      if (state_ == State::kWritingSmallLiteral) {
        if ((cur_literals_length_ + length) > 127) {
          if (puff_buf_out_ != nullptr) {
            // Boundary check
            TEST_AND_RETURN_FALSE(index_ + 2 <= puff_size_);

            // Shift two bytes forward to open space for length value.
            memmove(&puff_buf_out_[len_index_ + 3],
                    &puff_buf_out_[len_index_ + 1], cur_literals_length_);
          }
          index_ += 2;
          state_ = State::kWritingLargeLiteral;
        }
      }

      if (puff_buf_out_ != nullptr) {
        // Boundary check
        TEST_AND_RETURN_FALSE(index_ + length <= puff_size_);
        if (pd.type == PuffData::Type::kLiteral) {
          puff_buf_out_[index_] = pd.byte;
        } else {
          TEST_AND_RETURN_FALSE(pd.read_fn(&puff_buf_out_[index_], length));
        }
      } else if (pd.type == PuffData::Type::kLiterals) {
        TEST_AND_RETURN_FALSE(pd.read_fn(nullptr, length));
      }

      index_ += length;
      cur_literals_length_ += length;

      // Technically with the current structure of the puff stream, we cannot
      // have total length of more than 65663 bytes for a series of literals. So
      // we have to cap it at 65663 and continue afterwards.
      if (cur_literals_length_ == kLiteralsMaxLength) {
        TEST_AND_RETURN_FALSE(FlushLiterals());
      }
      break;
    }
    case PuffData::Type::kLenDist:
      TEST_AND_RETURN_FALSE(FlushLiterals());
      TEST_AND_RETURN_FALSE(pd.length <= 258 && pd.length >= 3);
      TEST_AND_RETURN_FALSE(pd.distance <= 32768 && pd.distance >= 1);
      if (pd.length < 130) {
        if (puff_buf_out_ != nullptr) {
          // Boundary check
          TEST_AND_RETURN_FALSE(index_ + 3 <= puff_size_);

          puff_buf_out_[index_++] =
              kLenDistHeader | static_cast<uint8_t>(pd.length - 3);
        } else {
          index_++;
        }
      } else {
        if (puff_buf_out_ != nullptr) {
          // Boundary check
          TEST_AND_RETURN_FALSE(index_ + 4 <= puff_size_);

          puff_buf_out_[index_++] = kLenDistHeader | 127;
          puff_buf_out_[index_++] = static_cast<uint8_t>(pd.length - 3 - 127);
        } else {
          index_ += 2;
        }
      }

      if (puff_buf_out_ != nullptr) {
        // Write the distance in the range [1..32768] zero-based.
        WriteUint16ToByteArray(pd.distance - 1, &puff_buf_out_[index_]);
      }
      index_ += 2;
      len_index_ = index_;
      state_ = State::kWritingNonLiteral;
      break;

    case PuffData::Type::kBlockMetadata:
      TEST_AND_RETURN_FALSE(FlushLiterals());
      TEST_AND_RETURN_FALSE(pd.length <= sizeof(pd.block_metadata) &&
                            pd.length > 0);
      if (puff_buf_out_ != nullptr) {
        // Boundary check
        TEST_AND_RETURN_FALSE(index_ + pd.length + 2 <= puff_size_);

        WriteUint16ToByteArray(pd.length - 1, &puff_buf_out_[index_]);
      }
      index_ += 2;

      if (puff_buf_out_ != nullptr) {
        memcpy(&puff_buf_out_[index_], pd.block_metadata, pd.length);
      }
      index_ += pd.length;
      len_index_ = index_;
      state_ = State::kWritingNonLiteral;
      break;

    case PuffData::Type::kEndOfBlock:
      TEST_AND_RETURN_FALSE(FlushLiterals());
      if (puff_buf_out_ != nullptr) {
        // Boundary check
        TEST_AND_RETURN_FALSE(index_ + 2 <= puff_size_);

        puff_buf_out_[index_++] = kLenDistHeader | 127;
        puff_buf_out_[index_++] = static_cast<uint8_t>(259 - 3 - 127);
      } else {
        index_ += 2;
      }

      len_index_ = index_;
      state_ = State::kWritingNonLiteral;
      break;

    default:
      return false;
  }
  return true;
}

bool BufferPuffWriter::FlushLiterals() {
  if (cur_literals_length_ == 0) {
    return true;
  }
  switch (state_) {
    case State::kWritingSmallLiteral:
      TEST_AND_RETURN_FALSE(cur_literals_length_ == (index_ - len_index_ - 1));
      if (puff_buf_out_ != nullptr) {
        puff_buf_out_[len_index_] =
            kLiteralsHeader | static_cast<uint8_t>(cur_literals_length_ - 1);
      }
      len_index_ = index_;
      state_ = State::kWritingNonLiteral;
      break;

    case State::kWritingLargeLiteral:
      TEST_AND_RETURN_FALSE(cur_literals_length_ == (index_ - len_index_ - 3));
      if (puff_buf_out_ != nullptr) {
        puff_buf_out_[len_index_++] = kLiteralsHeader | 127;
        WriteUint16ToByteArray(
            static_cast<uint16_t>(cur_literals_length_ - 127 - 1),
            &puff_buf_out_[len_index_]);
      }

      len_index_ = index_;
      state_ = State::kWritingNonLiteral;
      break;

    case State::kWritingNonLiteral:
      // Do nothing.
      break;

    default:
      return false;
  }
  cur_literals_length_ = 0;
  return true;
}

bool BufferPuffWriter::Flush() {
  TEST_AND_RETURN_FALSE(FlushLiterals());
  return true;
}

size_t BufferPuffWriter::Size() {
  return index_;
}

}  // namespace puffin
