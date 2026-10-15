/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_DECODER_CONFIG_FLAC_DECODER_CONFIG_H_
#define OBU_DECODER_CONFIG_FLAC_DECODER_CONFIG_H_

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief Strict constraints for the IAMF or FLAC spec.
 *
 * Typically these are "SHALL" requirements from either the FLAC or IAMF spec.
 * These are used to enforce in all modes.
 */
struct FlacStreamInfoStrictConstraints {
  // Required 0 audio_roll_distance as per IAMF spec.
  static constexpr int16_t kAudioRollDistance = 0;

  // The stream info block length is encoded using fields of fixed lengths.
  static constexpr uint16_t kStreamInfoBlockLength = 34;

  // Block size must be equal to num_samples_per_frame and at least 16, as per
  // FLAC spec.
  static constexpr uint16_t kMinMinAndMaxBlockSize = 16;

  // In IAMF the number_of_channels is fixed to `1`, but can be ignored when
  // reading / decoding. The actual number of channels is determined on a
  // per-substream basis based on the audio element.
  static constexpr uint8_t kNumberOfChannels = 1;

  // Acceptable minimum and maximum values for sample_rate, bits_per_sample. In
  // reality IAMF restricts these to discrete values.
  static constexpr uint32_t kMinSampleRate = 8000;
  static constexpr uint32_t kMaxSampleRate = 192000;
  // IAMF supports 16-bit through 32-bit FLAC.
  static constexpr uint8_t kMinBitsPerSample = 16;
  static constexpr uint8_t kMaxBitsPerSample = 32;
  // In the bitstream, bit-depth is serialized as one less than the value it
  // represents.
  static constexpr uint8_t kMinSerializedBitsPerSample = kMinBitsPerSample - 1;
  static constexpr uint8_t kMaxSerializedBitsPerSample = kMaxBitsPerSample - 1;

  // Acceptable ranges for totals_samples_in_stream from the FLAC documentation.
  // FLAC allows a value of 0 to represent an unknown total number of samples.
  static constexpr uint64_t kMinTotalSamplesInStream = 0;
  static constexpr uint64_t kMaxTotalSamplesInStream = 0xfffffffff;
};

/*!\brief Loose constraints for the IAMF or FLAC spec.
 *
 * Typically these are "SHOULD" requirements from either the FLAC or IAMF spec.
 * These requirements are enforced loosely to allow some components to handle
 * bitstreams which may not strictly follow the spec recommendations.
 */
struct FlacStreamInfoLooseConstraints {
  // The IAMF spec notes these SHOULD have fixed values.
  static constexpr uint32_t kMinFrameSize = 0;
  static constexpr uint32_t kMaxFrameSize = 0;

  // The IAMF spec notes that the md5_signature SHOULD be fixed to zero.
  static constexpr std::array<uint8_t, 16> kMd5Signature = {0};
};

/*!\brief Stream info portion of a metadata block described in the FLAC spec.
 *
 * Enumeration, ranges, and offsets are stored based on the values in FLAC spec.
 */
struct FlacMetaBlockStreamInfo {
  friend bool operator==(const FlacMetaBlockStreamInfo& lhs,
                         const FlacMetaBlockStreamInfo& rhs) = default;

  uint16_t minimum_block_size;
  uint16_t maximum_block_size;
  uint32_t minimum_frame_size =
      FlacStreamInfoLooseConstraints::kMinFrameSize;  // 24 bits.
  uint32_t maximum_frame_size =
      FlacStreamInfoLooseConstraints::kMaxFrameSize;  // 24 bits.
  uint32_t sample_rate;                               // 20 bits.
  uint8_t number_of_channels =
      FlacStreamInfoStrictConstraints::kNumberOfChannels;  // 3 bits.
  uint8_t bits_per_sample;                                 // 5 bits.
  uint64_t total_samples_in_stream;                        // 36 bits.
  std::array<uint8_t, 16> md5_signature =
      FlacStreamInfoLooseConstraints::kMd5Signature;
};

/*!\brief The header portion of a metadata block described in the FLAC spec. */
struct FlacMetaBlockHeader {
  /*!\brief An 8-bit enum for the type of FLAC block.
   *
   * See `BLOCK_TYPE` in the FLAC spec.
   */
  enum FlacBlockType : uint8_t {
    kFlacStreamInfo = 0,
    kFlacPadding = 1,
    kFlacApplication = 2,
    kFlacSeektable = 3,
    kFlacVorbisComment = 4,
    kFlacCuesheet = 5,
    kFlacPicture = 6,
    // 7 - 126 are reserved.
    kFlacInvalid = 127,
  };

  friend bool operator==(const FlacMetaBlockHeader& lhs,
                         const FlacMetaBlockHeader& rhs) = default;

  // `last_metadata_block_flag` is automatically inserted (1 bit).
  FlacBlockType block_type;  // 7 bits.
  // `metadata_data_block_length` is automatically inserted (24 bits).
};

struct FlacMetadataBlock {
  friend bool operator==(const FlacMetadataBlock& lhs,
                         const FlacMetadataBlock& rhs) = default;

  FlacMetaBlockHeader header;

  // When `header.block_type == kFlacStreamInfo` this is
  // `FlacMetaBlockStreamInfo`. Otherwise IAMF just passes along the data.
  std::variant<FlacMetaBlockStreamInfo, std::vector<uint8_t> > payload;
};
/*!\brief The `CodecConfig` `decoder_config` field for FLAC.*/
class FlacDecoderConfig {
 public:
  friend bool operator==(const FlacDecoderConfig& lhs,
                         const FlacDecoderConfig& rhs) = default;

  /*!\brief Returns the required audio roll distance.
   *
   * \return Audio roll distance required by the IAMF spec.
   */
  static int16_t GetRequiredAudioRollDistance() {
    return FlacStreamInfoStrictConstraints::kAudioRollDistance;
  }

  /*!\brief Validates and writes the `FlacDecoderConfig` to a buffer.
   *
   * \param num_samples_per_frame `num_samples_per_frame` in the associated
   *        Codec Config OBU.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific
   *         status on failure.
   */
  absl::Status ValidateAndWrite(uint32_t num_samples_per_frame,
                                WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the `FlacDecoderConfig` from a buffer.
   *
   * \param num_samples_per_frame `num_samples_per_frame` in the associated
   *        Codec Config OBU.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status ReadAndValidate(uint32_t num_samples_per_frame,
                               ReadBitBuffer& rb);

  /*!\brief Gets the output sample rate represented within the decoder config.
   *
   * This sample rate is used for timing and offset calculations.
   *
   * IAMF v1.1.0 section 3.11.3 specifies:
   *  > "The sample rate used for computing offsets SHALL be the sampling rate
   *     indicated in the METADATA_BLOCK."
   *
   * \param output_sample_rate Output sample rate.
   * \return `absl::OkStatus()` if successful.  `absl::InvalidArgumentError()`
   *         if the `FlacMetaBlockStreamInfo` cannot be found or if the
   *         retrieved value is invalid.
   */
  absl::Status GetOutputSampleRate(uint32_t& output_sample_rate) const;

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth the user should decode the signal
   * to.
   *
   * \param bit_depth_to_measure_loudness Bit-depth of the PCM which will be
   *        used to measure loudness.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if the `FlacMetaBlockStreamInfo`  cannot be found or if the
   *         retrieved value is invalid.
   */
  absl::Status GetBitDepthToMeasureLoudness(
      uint8_t& bit_depth_to_measure_loudness) const;

  /*!\brief Gets the `total_samples_in_stream` from a `FlacDecoderConfig`.
   *
   * \param total_samples_in_stream Total samples in stream.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if the `FlacMetaBlockStreamInfo`  cannot be found or if the
   *         retrieved value is invalid.
   */
  absl::Status GetTotalSamplesInStream(uint64_t& total_samples_in_stream) const;

  /*!\brief Prints logging information about the decoder config.
   */
  void Print() const;

  std::vector<FlacMetadataBlock> metadata_blocks_;
};

}  // namespace iamf_tools

#endif  // OBU_DECODER_CONFIG_FLAC_DECODER_CONFIG_H_
