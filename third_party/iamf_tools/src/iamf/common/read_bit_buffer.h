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
#ifndef COMMON_READ_BIT_BUFFER_H_
#define COMMON_READ_BIT_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Abstract class representing a buffer to read bits from.
 *
 * Concrete subclasses should hold the actual storage of the data and
 * implement `LoadBytesToBuffer()` to handle how data are loaded from the
 * storage to the internal buffer.
 */
class ReadBitBuffer {
 public:
  /*!\brief Destructor.*/
  virtual ~ReadBitBuffer() = default;

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        64.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 64` or the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer
   *         runs out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint64_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        32.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 32` or the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer
   *         runs out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint32_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        16.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 16` or the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer runs out of data
   *         and cannot get more from source before the desired `num_bits` are
   *         read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint16_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        8.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 8` or the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer runs
   *         out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint8_t& output);

  /*!\brief Reads the signed 16 bit integer from the read buffer.
   *
   * \param output Signed 16 bit integer will be written here.
   * \return `absl::OkStatus()` on success.  `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the signed 16 is fully read and
   *         source does not have the requisite data to complete the signed 16.
   *         `absl::InvalidArgumentError()` if the buffer's bit offset is
   *         negative.
   */
  absl::Status ReadSigned16(int16_t& output);

  /*!\brief Reads the signed 8 bit integer from the read buffer.
   *
   * \param output Signed 8 bit integer will be written here.
   * \return `absl::OkStatus()` on success.  `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the signed 8 is fully read and
   *         source does not have the requisite data to complete the signed 8.
   *         `absl::InvalidArgumentError()` if the buffer's bit offset is
   *         negative.
   */
  absl::Status ReadSigned8(int8_t& output);

  /*!\brief Reads the signed 9 bit integer from the read buffer.
   *
   * \param output Signed 9 bit integer will be written here.
   * \return `absl::OkStatus()` on success.  `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the signed 9 is fully read and
   *         source does not have the requisite data to complete the signed 9.
   *         `absl::InvalidArgumentError()` if the buffer's bit offset is
   *         negative.
   */
  absl::Status ReadSigned9(int16_t& output);

  /*!\brief Reads a null terminated string from the read buffer.
   *
   * \param output String will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the string is not terminated within `kIamfMaxStringSize` bytes.
   *         `absl::ResourceExhaustedError()` if the buffer is exhausted
   *         before the string is terminated and source does not have the
   *         requisite data to complete the string. Other specific statuses on
   *         failure.
   */
  absl::Status ReadString(std::string& output);

  /*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
   *
   * This version is useful when the caller does not care about the number of
   * bytes used to encode the data in the bitstream.
   *
   * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bits of
   *         uleb128, or if the data in the buffer requires that we read more
   *         than `kMaxLeb128Size` bytes, or the buffer's bit offset is
   *         negative. `absl::ResourceExhaustedError()` if the buffer is
   *         exhausted before the uleb128 is fully read and source does not
   *         have the requisite data to complete the uleb128.
   */
  absl::Status ReadULeb128(DecodedUleb128& uleb128);

  /*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
   *
   * This version also records the number of bytes used to store the encoded
   * uleb128 in the bitstream.
   *
   * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
   * \param encoded_uleb128_size Number of bytes used to store the encoded
   *        uleb128 in the bitstream.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bits of
   *         uleb128, or if the data in the buffer requires that we read more
   *         than `kMaxLeb128Size` bytes, the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the uleb128 is fully read and
   *         source does not have the requisite data to complete the uleb128.
   */
  absl::Status ReadULeb128(DecodedUleb128& uleb128,
                           int8_t& encoded_uleb128_size);

  /*!\brief Reads the expandable size according to ISO 14496-1.
   *
   * \param max_class_size Maximum class size in bits.
   * \param size_of_instance Size of instance according to the expandable size.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bit
   *         output, or if the data encoded is larger than the `max_class_size`
   *         bits, the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer is exhausted
   *         before the expanded field is fully read and source does not have
   *         the requisite data to complete the expanded field.
   */
  absl::Status ReadIso14496_1Expanded(uint32_t max_class_size,
                                      uint32_t& size_of_instance);

  /*!\brief Reads `uint8_t`s into the output span.
   *
   * \param output Span of `uint8_t`s to write to.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data and cannot get more from source before
   *         filling the span. `absl::InvalidArgumentError()` if the
   *         buffer's bit offset is negative.
   */
  absl::Status ReadUint8Span(absl::Span<uint8_t> output);

  /*!\brief Reads a boolean from buffer into `output`.
   *
   * \param output Boolean bit from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data and cannot get more from source before
   *         the desired boolean is read. `absl::InvalidArgumentError()` if the
   *         buffer's bit offset is negative.
   */
  absl::Status ReadBoolean(bool& output);

  /*!\brief Checks whether there is any data left in the buffer or source.
   *
   * \return `true` if there is some data left in the buffer or source that has
   *         not been consumed yet. `false` otherwise.
   */
  bool IsDataAvailable() const;

  /*!\brief Returns the number of bytes available in the buffer.
   *
   * \return Number of bytes available in the buffer.
   */
  int64_t NumBytesAvailable() const;

  /*!\brief Returns the next reading position of the source in bits.
   *
   * \return Next reading position of the source in bits.
   */
  int64_t Tell();

  /*!\brief Moves the next reading position in bits of the source.
   *
   * It is OK to move exactly to the end of the source, in which case further
   * reads will fail, until the reading position is moved back, or additional
   * data is provided.
   *
   * \param position Requested position in bits to move to.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data. `absl::InvalidArgumentError()` if
   *         the requested position is negative.
   */
  absl::Status Seek(int64_t position);

  /*!\brief Consumes and ignores `num_bytes` bytes from the source.
   *
   * \param num_bytes Number of bytes to ignore.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data.
   */
  absl::Status IgnoreBytes(int64_t num_bytes);

 protected:
  /*!\brief Constructor.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   * \param source_size Size of the source data in bits.
   */
  ReadBitBuffer(size_t capacity_bytes, int64_t source_size_bits);

  /*!\brief Internal reading function that reads `num_bits` from buffer.
   *
   * As a side effect buffer loading might happen.
   *
   * \param num_bits Number of upper bits to read from buffer.
   * \param max_num_bits Maximum number of upper bits to read from buffer.
   * \param output Output unsigned literal read from buffer.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > max_num_bits` or the buffer's bit offset is negative.
   *         `absl::ResourceExhaustedError()` if the buffer runs out of data
   *         and cannot get more from source before the desired `num_bits`
   *         are read.
   */
  absl::Status ReadUnsignedLiteralInternal(int num_bits, int max_num_bits,
                                           uint64_t& output);
  /*!\brief Load bytes from source to the buffer.
   *
   * Subclasses of this class should implement the actual loading logic.
   *
   * \param starting_byte Starting byte to load from source.
   * \param num_bytes Number of bytes to load.
   * \return `absl::OkStatus()` on success. Other specific statuses (depending
   *         on the subclass) on failure.
   */
  virtual absl::Status LoadBytesToBuffer(int64_t starting_byte,
                                         int64_t num_bytes) = 0;

  // Read buffer.
  std::vector<uint8_t> bit_buffer_;

  // Specifies the next bit to consume in the `bit_buffer_`.
  int64_t buffer_bit_offset_ = 0;

  // Size of the valid data in the buffer in bits.
  int64_t buffer_size_bits_ = 0;

  // Size of the source data in bits. It may refer to the total file size
  // for a file-based buffer, or the total memory size for a memory-based
  // buffer. For a stream-based buffer, it is the current size of the source
  // data, which is updated as bytes are pushed or flushed.
  int64_t source_size_bits_;

  // Specifies the next bit to consume from the source data (the actual storage
  // type is subclass-specific).
  int64_t source_bit_offset_ = 0;

  // Specifies whether a position returned by Tell() is valid.
  bool is_position_valid_;
};

/*!\brief Memory-based read bit buffer.
 *
 * The entire content of the source data is held as a vector inside the class.
 *
 * NOTICE: This is mostly useful for testing and processing small files,
 * because it will hold the entire content in memory during its lifetime.
 * For processing large (e.g. 2 GB) files, use the `FileBasedReadBitBuffer`
 * for example.
 */
class MemoryBasedReadBitBuffer : public ReadBitBuffer {
 public:
  /*!\brief Creates an instance of a memory-based read bit buffer.
   *
   * \param source Source span from which the buffer will load data. The
   *        entire contents will be copied into the constructed instance.
   * \return Unique pointer of the created instance. `nullptr` if the creation
   *         fails.
   */
  static std::unique_ptr<MemoryBasedReadBitBuffer> CreateFromSpan(
      absl::Span<const uint8_t> source);

  /*!\brief Destructor.*/
  ~MemoryBasedReadBitBuffer() override = default;

 protected:
  /*!\brief Protected constructor. Called by the factory method or subclasses.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   * \param source Source span from which the buffer will load data. The
   *        entire contents will be copied into the constructed instance.
   */
  MemoryBasedReadBitBuffer(size_t capacity_bytes,
                           absl::Span<const uint8_t> source);
  /*!\brief Load bytes from the source vector to the buffer.
   *
   * \param starting_byte Starting byte to load from source.
   * \param num_bytes Number of bytes to load.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the start/ending position is invalid.
   */
  absl::Status LoadBytesToBuffer(int64_t starting_byte,
                                 int64_t num_bytes) override;

  // Source data stored in a vector.
  std::vector<uint8_t> source_vector_;
};

/*!\brief File-based read bit buffer.
 *
 * The file is read and the buffer is loaded only when necessary.
 */
class FileBasedReadBitBuffer : public ReadBitBuffer {
 public:
  /*!\brief Creates an instance of a file-based read bit buffer.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   * \param file_path Path to the file to load the buffer from.
   * \return Unique pointer of the created instance. `nullptr` if the creation
   *         fails.
   */
  static std::unique_ptr<FileBasedReadBitBuffer> CreateFromFilePath(
      int64_t capacity_bytes, const std::filesystem::path& file_path);

  /*!\brief Destructor.*/
  ~FileBasedReadBitBuffer() override = default;

 protected:
  /*!\brief Protected constructor. Called by the factory method or subclasses.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   * \param source_size Total size of the file in bits.
   * \param ifs Input file stream from which the buffer will load data. At most
   *        a buffer full of data will be read at a given time.
   *
   */
  FileBasedReadBitBuffer(size_t capacity_bytes, int64_t source_size,
                         std::ifstream&& ifs);

  /*!\brief Load bytes from the source file to the buffer.
   *
   * \param starting_byte Starting byte to load from source.
   * \param num_bytes Number of bytes to load.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the file reading fails.
   */
  absl::Status LoadBytesToBuffer(int64_t starting_byte,
                                 int64_t num_bytes) override;

 private:
  // Source data stored in a file stream.
  std::ifstream source_ifs_;
};

/*!\brief Stream-based read bit buffer.
 *
 * The buffer is loaded from a stream. The user should Create() the stream
 * and push data to the buffer using PushBytes() as needed; calls to Read*()
 * methods will read data from the stream and provide it to the caller, or else
 * will instruct the caller to push more data if necessary.
 */
class StreamBasedReadBitBuffer : public MemoryBasedReadBitBuffer {
 public:
  /*!\brief Creates an instance of a stream-based read bit buffer.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   * \return Unique pointer of the created instance. `nullptr` if the creation
   *         fails.
   */
  static std::unique_ptr<StreamBasedReadBitBuffer> Create(
      int64_t capacity_bytes);

  /*!\brief Adds some chunk of data to StreamBasedReadBitBuffer.
   *
   * \param bytes Bytes to push.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the stream push fails.
   */
  absl::Status PushBytes(absl::Span<const uint8_t> bytes);

  /*!\brief Flush already processed data from StreamBasedReadBitBuffer.
   *
   * Flushes any already processed data up to the most recently read byte.
   */
  void Flush();

  /*!\brief Destructor.*/
  ~StreamBasedReadBitBuffer() override = default;

 private:
  /*!\brief Private constructor.
   *
   * \param capacity_bytes Capacity of the internal buffer in bytes.
   */
  StreamBasedReadBitBuffer(size_t capacity_bytes);

  // Specifies the maximum size of the source data in bits.
  int64_t max_source_size_bits_;
};

}  // namespace iamf_tools

#endif  // COMMON_READ_BIT_BUFFER_H_
