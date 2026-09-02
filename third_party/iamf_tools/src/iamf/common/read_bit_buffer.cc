/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/common/read_bit_buffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

bool CanReadByteAligned(const int64_t& buffer_bit_offset,
                        const int64_t& num_bits) {
  const bool buffer_bit_offset_is_aligned = (buffer_bit_offset % 8 == 0);
  const bool num_bits_to_read_is_aligned = (num_bits % 8 == 0);
  return buffer_bit_offset_is_aligned && num_bits_to_read_is_aligned;
}

// Reads one bit from source_data at position `offset`. Reads in order of most
// significant to least significant - that is, offset = 0 refers to the bit in
// position 2^7, offset = 1 refers to the bit in position 2^6, etc. Caller
// should ensure that offset/8 is < data.size().
uint8_t GetUpperBit(const int64_t& offset,
                    const std::vector<uint8_t>& source_data) {
  int64_t byte_index = offset / 8;
  uint8_t bit_index = 7 - (offset % 8);
  return (source_data.at(byte_index) >> bit_index) & 0x01;
}

// Read unsigned literal bit by bit. Data is read into the lower
// `remaining_bits_to_read` of `output` from the upper `remaining_bits_to_read`
// of bit_offer[buffer_bit_offset].
//
// Ex: Input: bit_buffer = 10000111, buffer_bit_offset = 0,
//        remaining_bits_to_read = 5, output = 0
//     Output: output = {59 leading zeroes} + 10000, buffer_bit_offset = 5,
//        remaining_bits_to_read = 0.
void ReadUnsignedLiteralBits(const std::vector<uint8_t>& bit_buffer,
                             const int64_t buffer_size,
                             int64_t& buffer_bit_offset,
                             int64_t& remaining_bits_to_read,
                             uint64_t& output) {
  while ((static_cast<uint64_t>(buffer_bit_offset / 8) < bit_buffer.size()) &&
         remaining_bits_to_read > 0 && (buffer_bit_offset < buffer_size)) {
    uint8_t upper_bit = GetUpperBit(buffer_bit_offset, bit_buffer);
    output <<= 1;
    output |= static_cast<uint64_t>(upper_bit);
    remaining_bits_to_read--;
    buffer_bit_offset++;
  }
}

// Read unsigned literal byte by byte.
void ReadUnsignedLiteralBytes(const std::vector<uint8_t>& bit_buffer,
                              int64_t& buffer_bit_offset,
                              int64_t& remaining_bits_to_read,
                              uint64_t& output) {
  while ((static_cast<uint64_t>(buffer_bit_offset / 8) < bit_buffer.size()) &&
         remaining_bits_to_read > 0) {
    output <<= 8;
    output |= static_cast<uint64_t>(bit_buffer.at(buffer_bit_offset / 8));
    remaining_bits_to_read -= 8;
    buffer_bit_offset += 8;
  }
}

typedef absl::AnyInvocable<void(uint64_t, int, uint64_t&) const>
    ByteAccumulator;

absl::Status AccumulateUleb128Byte(const ByteAccumulator& accumulator,
                                   uint32_t max_output, const uint64_t& byte,
                                   const int index, bool& is_terminal_block,
                                   uint64_t& accumulated_value) {
  accumulator(byte, index, accumulated_value);
  is_terminal_block = ((byte & 0x80) == 0);
  if ((index == (kMaxLeb128Size - 1)) && !is_terminal_block) {
    return absl::InvalidArgumentError(
        "Have read the max allowable bytes for a uleb128, but bitstream "
        "says to keep reading.");
  }
  if (accumulated_value > max_output) {
    return absl::InvalidArgumentError(
        absl::StrCat("Overflow - data is larger than max_output=", max_output));
  }
  return absl::OkStatus();
}

// Common internal function for reading uleb128 and iso14496_1 expanded. They
// have similar logic except the bytes are accumulated in different orders, and
// they have different max output values.
absl::Status AccumulateUleb128OrIso14496_1Internal(
    const ByteAccumulator& accumulator, const uint32_t max_output,
    ReadBitBuffer& rb, uint32_t& output, int8_t& encoded_size) {
  uint64_t accumulated_value = 0;
  uint64_t byte = 0;
  bool terminal_block = false;
  encoded_size = 0;
  for (int i = 0; i < kMaxLeb128Size; ++i) {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, byte));
    encoded_size++;
    RETURN_IF_NOT_OK(AccumulateUleb128Byte(accumulator, max_output, byte, i,
                                           terminal_block, accumulated_value));

    if (terminal_block) {
      break;
    }
  }
  // Accumulated value is guaranteed to fit into a `uint32_t` at this
  // stage.
  output = static_cast<uint32_t>(accumulated_value);
  return absl::OkStatus();
}

}  // namespace

// Reads n = `num_bits` bits from the buffer. These are the upper n bits of
// `bit_buffer_`. n must be <= 64. The read data is consumed, meaning
// `buffer_bit_offset_` is incremented by n as a side effect of this fxn.
absl::Status ReadBitBuffer::ReadUnsignedLiteral(const int num_bits,
                                                uint64_t& output) {
  return ReadUnsignedLiteralInternal(num_bits, 64, output);
}

absl::Status ReadBitBuffer::ReadUnsignedLiteral(const int num_bits,
                                                uint32_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteralInternal(num_bits, 32, value));
  output = static_cast<uint32_t>(value);
  return absl::OkStatus();
}

absl::Status ReadBitBuffer::ReadUnsignedLiteral(const int num_bits,
                                                uint16_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteralInternal(num_bits, 16, value));
  output = static_cast<uint16_t>(value);
  return absl::OkStatus();
}

absl::Status ReadBitBuffer::ReadUnsignedLiteral(const int num_bits,
                                                uint8_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteralInternal(num_bits, 8, value));
  output = static_cast<uint8_t>(value);
  return absl::OkStatus();
}

// Reads a standard int16_t in two's complement form from the read buffer.
absl::Status ReadBitBuffer::ReadSigned16(int16_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteral(16, value));
  output = static_cast<int16_t>(value) & 0xffff;
  return absl::OkStatus();
}

// Reads an 8-bit signed integer in two's complement form from the read buffer.
absl::Status ReadBitBuffer::ReadSigned8(int8_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteral(8, value));
  output = static_cast<int8_t>(value);
  return absl::OkStatus();
}

// Reads a 9-bit signed integer in two's complement form from the read buffer.
absl::Status ReadBitBuffer::ReadSigned9(int16_t& output) {
  uint64_t value;
  RETURN_IF_NOT_OK(ReadUnsignedLiteral(9, value));
  // Sign extend from 9 bits to 16 bits.
  if (value & 0x100) {
    output = static_cast<int16_t>(value | 0xfe00);
  } else {
    output = static_cast<int16_t>(value);
  }
  return absl::OkStatus();
}

// Reads a null terminated C-style string from the buffer.
absl::Status ReadBitBuffer::ReadString(std::string& output) {
  // Read up to the first `kIamfMaxStringSize` characters. Exit after seeing the
  // null terminator. Override anything in `output`.
  output = "";
  for (int i = 0; i < kIamfMaxStringSize; i++) {
    uint8_t byte;
    RETURN_IF_NOT_OK(ReadUnsignedLiteral(8, byte));
    if (byte == '\0') {
      return absl::OkStatus();
    }
    output.push_back(byte);
  }

  // Failed to find the null terminator within `kIamfMaxStringSize` bytes.
  return absl::InvalidArgumentError(
      "Failed to find the null terminator for data= ");
}

absl::Status ReadBitBuffer::ReadULeb128(DecodedUleb128& uleb128) {
  int8_t unused_size;
  return ReadULeb128(uleb128, unused_size);
}

absl::Status ReadBitBuffer::ReadULeb128(DecodedUleb128& uleb128,
                                        int8_t& encoded_uleb128_size) {
  const ByteAccumulator little_endian_accumulator =
      [&](uint64_t byte, int index, uint64_t& accumulated_value) {
        accumulated_value |= (byte & 0x7f) << (7 * index);
      };
  // IAMF requires all `leb128`s to decode to a value that fits in 32 bits.
  const uint32_t kMaxUleb128 = std::numeric_limits<uint32_t>::max();
  return AccumulateUleb128OrIso14496_1Internal(little_endian_accumulator,
                                               kMaxUleb128, *this, uleb128,
                                               encoded_uleb128_size);
}

absl::Status ReadBitBuffer::ReadIso14496_1Expanded(uint32_t max_class_size,
                                                   uint32_t& size_of_instance) {
  const ByteAccumulator big_endian_accumulator =
      [](uint64_t byte, int /*index*/, uint64_t& accumulated_value) {
        accumulated_value = accumulated_value << 7 | (byte & 0x7f);
      };
  int8_t unused_encoded_size = 0;
  return AccumulateUleb128OrIso14496_1Internal(
      big_endian_accumulator, max_class_size, *this, size_of_instance,
      unused_encoded_size);
}

absl::Status ReadBitBuffer::ReadUint8Span(absl::Span<uint8_t> output) {
  for (auto& byte : output) {
    RETURN_IF_NOT_OK(ReadUnsignedLiteral(8, byte));
  }
  return absl::OkStatus();
}

absl::Status ReadBitBuffer::ReadBoolean(bool& output) {
  uint64_t bit;
  RETURN_IF_NOT_OK(ReadUnsignedLiteral(1, bit));
  output = static_cast<bool>(bit);
  return absl::OkStatus();
}

ReadBitBuffer::ReadBitBuffer(size_t capacity_bytes, int64_t source_size_bits)
    : bit_buffer_(capacity_bytes),
      buffer_bit_offset_(0),
      buffer_size_bits_(0),
      source_size_bits_(source_size_bits),
      source_bit_offset_(0) {}

bool ReadBitBuffer::IsDataAvailable() const { return NumBytesAvailable() > 0; }

int64_t ReadBitBuffer::NumBytesAvailable() const {
  ABSL_CHECK(source_bit_offset_ >= 0 &&
             source_bit_offset_ <= source_size_bits_);
  const int64_t num_bytes_in_source =
      (source_size_bits_ - source_bit_offset_) / 8;
  ABSL_CHECK(buffer_bit_offset_ >= 0 &&
             buffer_bit_offset_ <= buffer_size_bits_);
  const int64_t num_bytes_in_buffer =
      (buffer_size_bits_ - buffer_bit_offset_) / 8;
  return num_bytes_in_source + num_bytes_in_buffer;
}

int64_t ReadBitBuffer::Tell() {
  is_position_valid_ = true;
  return source_bit_offset_ - buffer_size_bits_ + buffer_bit_offset_;
}

absl::Status ReadBitBuffer::Seek(const int64_t position) {
  if (!is_position_valid_) {
    return absl::FailedPreconditionError(
        "Seeking to position has been disabled. This can happen if Flush() has "
        "been called after Tell().");
  }
  if (position < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid source position: ", position));
  }

  if (position > source_size_bits_) {
    return absl::ResourceExhaustedError(
        absl::StrCat("Not enough bits in source: position= ", position,
                     " > #(bits in source)= ", source_size_bits_));
  }

  // Simply move the `buffer_bit_offset_` if the requested position lies
  // within the current buffer.
  if ((source_bit_offset_ - buffer_size_bits_ <= position) &&
      (position < source_bit_offset_)) {
    buffer_bit_offset_ = position - (source_bit_offset_ - buffer_size_bits_);
    return absl::OkStatus();
  }

  // Load the data from the source, starting from the byte that the requested
  // position is at.
  const int64_t starting_byte = position / 8;
  const int64_t num_bytes =
      std::min(static_cast<int64_t>(bit_buffer_.capacity()),
               source_size_bits_ / 8 - starting_byte);

  RETURN_IF_NOT_OK(LoadBytesToBuffer(starting_byte, num_bytes));

  // Update other bookkeeping data.
  buffer_bit_offset_ = position % 8;
  source_bit_offset_ = (starting_byte + num_bytes) * 8;
  buffer_size_bits_ = num_bytes * 8;

  return absl::OkStatus();
}

absl::Status ReadBitBuffer::IgnoreBytes(int64_t num_bytes) {
  return Seek(Tell() + num_bytes * 8);
}

absl::Status ReadBitBuffer::ReadUnsignedLiteralInternal(const int num_bits,
                                                        const int max_num_bits,
                                                        uint64_t& output) {
  if (num_bits > max_num_bits) {
    return absl::InvalidArgumentError("num_bits must be <= max_num_bits.");
  }
  if (num_bits < 0) {
    return absl::InvalidArgumentError("num_bits must be >= 0.");
  }
  if (buffer_bit_offset_ < 0) {
    return absl::InvalidArgumentError("buffer_bit_offset_ must be >= 0.");
  }
  if (Tell() + num_bits > source_size_bits_) {
    return absl::ResourceExhaustedError("Not enough bits to read");
  }
  output = 0;

  // Early return if 0 bit is requested to be read.
  if (num_bits == 0) {
    return absl::OkStatus();
  }

  // Now at least one bit is needed, make sure the buffer has some data in it.
  RETURN_IF_NOT_OK(Seek(Tell()));
  int64_t remaining_bits_to_read = num_bits;
  const int64_t expected_final_position = Tell() + remaining_bits_to_read;

  // If the final position and the current position lies within the same byte.
  if (expected_final_position / 8 == Tell() / 8) {
    ReadUnsignedLiteralBits(bit_buffer_, buffer_size_bits_, buffer_bit_offset_,
                            remaining_bits_to_read, output);
    ABSL_CHECK_EQ(remaining_bits_to_read, 0) << remaining_bits_to_read;
    return absl::OkStatus();
  }

  // Read the first several bits so that the `buffer_bit_offset_` is byte
  // aligned.
  if (buffer_bit_offset_ % 8 != 0) {
    int64_t num_bits_to_byte_aligned = 8 - (buffer_bit_offset_ % 8);
    remaining_bits_to_read -= num_bits_to_byte_aligned;
    ReadUnsignedLiteralBits(bit_buffer_, buffer_size_bits_, buffer_bit_offset_,
                            num_bits_to_byte_aligned, output);
  }

  // Read consecutive complete bytes.
  while (remaining_bits_to_read >= 8) {
    // Make sure the reading position has some buffer to read if possible.
    RETURN_IF_NOT_OK(Seek(Tell()));

    // Read as much as possible from the buffer.
    int64_t num_bits_from_buffer =
        std::min(buffer_size_bits_ - buffer_bit_offset_,
                 (remaining_bits_to_read / 8) * 8);

    ABSL_CHECK(CanReadByteAligned(buffer_bit_offset_, num_bits_from_buffer));
    remaining_bits_to_read -= num_bits_from_buffer;
    ReadUnsignedLiteralBytes(bit_buffer_, buffer_bit_offset_,
                             num_bits_from_buffer, output);
  }

  // Read the final several bits in the last byte.
  int64_t num_bits_in_final_byte = expected_final_position % 8;
  remaining_bits_to_read -= num_bits_in_final_byte;
  RETURN_IF_NOT_OK(Seek(Tell()));
  ReadUnsignedLiteralBits(bit_buffer_, buffer_size_bits_, buffer_bit_offset_,
                          num_bits_in_final_byte, output);
  ABSL_CHECK_EQ(remaining_bits_to_read, 0) << remaining_bits_to_read;
  return absl::OkStatus();
}

// ----- MemoryBasedReadBitBuffer -----

std::unique_ptr<MemoryBasedReadBitBuffer>
MemoryBasedReadBitBuffer::CreateFromSpan(absl::Span<const uint8_t> source) {
  return absl::WrapUnique(new MemoryBasedReadBitBuffer(source.size(), source));
}

absl::Status MemoryBasedReadBitBuffer::LoadBytesToBuffer(int64_t starting_byte,
                                                         int64_t num_bytes) {
  if (starting_byte < 0 || num_bytes < 0 ||
      static_cast<uint64_t>(num_bytes) > bit_buffer_.size()) {
    return absl::InvalidArgumentError(
        "Invalid source position or destination buffer size");
  }
  if (static_cast<uint64_t>(starting_byte) > source_vector_.size() ||
      (static_cast<uint64_t>(starting_byte) + num_bytes) >
          source_vector_.size()) {
    return absl::InvalidArgumentError(
        "Invalid starting or ending position to read from the vector");
  }

  std::copy(source_vector_.begin() + starting_byte,
            source_vector_.begin() + starting_byte + num_bytes,
            bit_buffer_.begin());
  return absl::OkStatus();
}

MemoryBasedReadBitBuffer::MemoryBasedReadBitBuffer(
    size_t capacity_bytes, absl::Span<const uint8_t> source)
    : ReadBitBuffer(capacity_bytes, static_cast<int64_t>(source.size()) * 8),
      source_vector_(source.begin(), source.end()) {}

// ----- FileBasedReadBitBuffer -----

std::unique_ptr<FileBasedReadBitBuffer>
FileBasedReadBitBuffer::CreateFromFilePath(
    const int64_t capacity_bytes, const std::filesystem::path& file_path) {
  if (capacity_bytes < 0) {
    ABSL_LOG(ERROR) << "FileBasedReadBitBuffer capacity_bytes must be >= 0.";
    return nullptr;
  }
  if (!std::filesystem::exists(file_path)) {
    ABSL_LOG(ERROR) << "File not found: " << file_path;
    return nullptr;
  }
  std::ifstream ifs(file_path, std::ios::binary | std::ios::in);
  ifs.seekg(0, ifs.end);
  const auto file_size = static_cast<size_t>(ifs.tellg());
  ifs.seekg(0, ifs.beg);
  if (!ifs.good()) {
    ABSL_LOG(ERROR) << "Error accessing " << file_path;
    return nullptr;
  }

  // File size is in bytes, `source_size` is in bits.
  return absl::WrapUnique(new FileBasedReadBitBuffer(
      capacity_bytes, file_size * 8, std::move(ifs)));
}

absl::Status FileBasedReadBitBuffer::LoadBytesToBuffer(int64_t starting_byte,
                                                       int64_t num_bytes) {
  if (starting_byte < 0 || num_bytes < 0 ||
      static_cast<uint64_t>(num_bytes) > bit_buffer_.size()) {
    return absl::InvalidArgumentError(
        "Invalid source position or destination buffer size");
  }
  source_ifs_.seekg(starting_byte);
  source_ifs_.read(reinterpret_cast<char*>(bit_buffer_.data()), num_bytes);
  if (!source_ifs_.good()) {
    return absl::InvalidArgumentError(
        absl::StrCat("File reading failed. State= ", source_ifs_.rdstate()));
  }

  return absl::OkStatus();
}

FileBasedReadBitBuffer::FileBasedReadBitBuffer(size_t capacity_bytes,
                                               int64_t source_size,
                                               std::ifstream&& ifs)
    : ReadBitBuffer(capacity_bytes, source_size), source_ifs_(std::move(ifs)) {}

// ----- StreamBasedReadBitBuffer -----
std::unique_ptr<StreamBasedReadBitBuffer> StreamBasedReadBitBuffer::Create(
    int64_t capacity_bytes) {
  if (capacity_bytes < 0) {
    ABSL_LOG(ERROR) << "StreamBasedReadBitBuffer capacity_bytes must be >= 0.";
    return nullptr;
  }
  // Since this is a stream based buffer, we do not initialize with any data.
  return absl::WrapUnique(new StreamBasedReadBitBuffer(capacity_bytes));
}

absl::Status StreamBasedReadBitBuffer::PushBytes(
    absl::Span<const uint8_t> bytes) {
  if (bytes.size() > ((max_source_size_bits_ / 8) - source_vector_.size())) {
    return absl::InvalidArgumentError(
        "Cannot push more bytes than the available space in the source.");
  }
  // Copy the bytes to the source vector; this is added to the end in case there
  // are already some bytes in the source.
  source_vector_.insert(source_vector_.end(), bytes.begin(), bytes.end());
  // The source grows as bytes are pushed.
  source_size_bits_ += bytes.size() * 8;
  return absl::OkStatus();
}

// Flush should be called in a reasonable manner, i.e. not every time a read
// operation is performed, as the removal of elements from the source vector
// is an O(n) operation, where n is the number of elements in the source vector.
// In general, it is a trade-off: the more often it is called, the more
// space-efficient the buffer is (since it holds less data in the source vector)
// while simultaneously being less time-efficient (since it takes time to remove
// elements from the source vector).
void StreamBasedReadBitBuffer::Flush() {
  // Intentionally a floored result from integer division. Any partially read
  // byte will remain and be handled by `source_bit_offset_` internal
  // bookkeeping.
  const int64_t num_bytes = Tell() / 8;
  // Tell should never return a value larger than the source vector size.
  ABSL_CHECK_LE(static_cast<uint64_t>(num_bytes), source_vector_.size())
      << "Tell() returned a value larger than the source vector size (in "
         "bytes).";
  source_vector_.erase(source_vector_.begin(),
                       source_vector_.begin() + num_bytes);
  // Offset needs to be moved back as erase moves the elements of a vector that
  // are not removed to the beginning of the vector.
  source_bit_offset_ -= num_bytes * 8;
  source_size_bits_ -= num_bytes * 8;
  // Disable seeking as the position returned by a previous Tell() call is no
  // longer valid.
  is_position_valid_ = false;
}

StreamBasedReadBitBuffer::StreamBasedReadBitBuffer(size_t capacity_bytes)
    : MemoryBasedReadBitBuffer(capacity_bytes, absl::Span<const uint8_t>()) {
  max_source_size_bits_ = kEntireObuSizeMaxTwoMegabytes * 2 * 8;
}

}  // namespace iamf_tools
