// Copyright 2019 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stream/base94_output_stream.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

namespace {
// To improve the space efficiency, we can encode 14 bits into two symbols
// if 14-bit number is less than 94^2 which is total number can be encoded
// by 2 digits of base94 number, in another word, if 13 bit number is
// smaller than 643, we could read one more bit, because even if the 14th
// bit is 1, the 14-bit number doesn’t exceed the max value.
constexpr uint16_t kMaxValueOf14BitEncoding = (94 * 94 - 1) & 0x1FFF;

constexpr size_t kMaxBuffer = 4096;

inline uint8_t EncodeByte(uint8_t byte) {
  DCHECK(byte < 94);
  return (byte >= 94u) ? 0xff : (byte + '!');
}

inline uint8_t DecodeByte(uint8_t byte) {
  DCHECK(byte >= '!' && byte <= '~');
  return std::min(static_cast<uint8_t>(byte - '!'), static_cast<uint8_t>(94));
}

}  // namespace

Base94OutputStream::Base94OutputStream(
    Mode mode,
    std::unique_ptr<OutputStreamInterface> output_stream)
    : mode_(mode),
      output_stream_(std::move(output_stream)),
      bit_buf_(0),
      bit_count_(0),
      symbol_buffer_(0),
      flush_needed_(false),
      flushed_(false) {
  buffer_.reserve(kMaxBuffer);
}

Base94OutputStream::~Base94OutputStream() {
  DCHECK(!flush_needed_);
}

bool Base94OutputStream::Write(const uint8_t* data, size_t size) {
  DCHECK(!flushed_);
  flush_needed_ = true;
  return mode_ == Mode::kEncode ? Encode(data, size) : Decode(data, size);
}

bool Base94OutputStream::Flush() {
  flushed_ = true;
  if (flush_needed_) {
    flush_needed_ = false;
    if (!((mode_ == Mode::kEncode) ? FinishEncoding() : FinishDecoding()))
      return false;
  }
  return output_stream_->Flush();
}

bool Base94OutputStream::Encode(const uint8_t* data, size_t size) {
  const uint8_t* cur = data;
  while (size--) {
    bit_buf_ |= *(cur++) << bit_count_;
    bit_count_ += 8;
    if (bit_count_ < 14)
      continue;

    uint16_t block;
    // Check if 13-bit or 14-bit data should be encoded.
    if ((bit_buf_ & 0x1FFF) > kMaxValueOf14BitEncoding) {
      block = bit_buf_ & 0x1FFF;
      bit_buf_ >>= 13;
      bit_count_ -= 13;
    } else {
      block = bit_buf_ & 0x3FFF;
      bit_buf_ >>= 14;
      bit_count_ -= 14;
    }
    buffer_.push_back(EncodeByte(block % 94));
    buffer_.push_back(EncodeByte(base::saturated_cast<uint8_t>(block / 94)));

    if (buffer_.size() > kMaxBuffer - 2 && !WriteOutputStream())
      return false;
  }
  return WriteOutputStream();
}

bool Base94OutputStream::Decode(const uint8_t* data, size_t size) {
  const uint8_t* cur = data;
  while (size--) {
    if (DecodeByte(*cur) == 94) {
      LOG(ERROR) << "Decode: invalid input";
      return false;
    }
    if (symbol_buffer_ == 0) {
      symbol_buffer_ = *cur;
      cur++;
      continue;
    }
    uint16_t v = DecodeByte(symbol_buffer_) + DecodeByte(*cur) * 94;
    cur++;
    symbol_buffer_ = 0;
    bit_buf_ |= v << bit_count_;
    bit_count_ += (v & 0x1FFF) > kMaxValueOf14BitEncoding ? 13 : 14;
    while (bit_count_ > 7) {
      buffer_.push_back(bit_buf_ & 0xff);
      bit_buf_ >>= 8;
      bit_count_ -= 8;
    }
    if (buffer_.size() > kMaxBuffer - 2) {
      if (!WriteOutputStream())
        return false;
    }
  }
  return WriteOutputStream();
}

bool Base94OutputStream::FinishEncoding() {
  if (bit_count_ == 0)
    return true;
  // Up to 13 bits data is left over.
  buffer_.push_back(EncodeByte(bit_buf_ % 94));
  if (bit_buf_ > 93 || bit_count_ > 8)
    buffer_.push_back(EncodeByte(base::saturated_cast<uint8_t>(bit_buf_ / 94)));
  bit_count_ = 0;
  bit_buf_ = 0;
  return WriteOutputStream();
}

bool Base94OutputStream::FinishDecoding() {
  // The left over bit is padding and all zero, if there is no symbol
  // unprocessed.
  if (symbol_buffer_ == 0) {
    DCHECK(!bit_buf_);
    return true;
  }
  bit_buf_ |= DecodeByte(symbol_buffer_) << bit_count_;
  buffer_.push_back(bit_buf_ & 0xff);
  bit_buf_ >>= 8;
  // The remaining bits are either encode padding or zeros from bit shift.
  DCHECK(!bit_buf_);
  return WriteOutputStream();
}

bool Base94OutputStream::WriteOutputStream() {
  if (buffer_.empty())
    return true;

  bool result = output_stream_->Write(buffer_.data(), buffer_.size());
  buffer_.clear();
  return result;
}

}  // namespace crashpad
