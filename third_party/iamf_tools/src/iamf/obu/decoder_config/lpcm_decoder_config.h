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
#ifndef OBU_DECODER_CONFIG_LPCM_DECODER_CONFIG_H_
#define OBU_DECODER_CONFIG_LPCM_DECODER_CONFIG_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief The `CodecConfig` `decoder_config` field for LPCM. */
class LpcmDecoderConfig {
 public:
  /*!\brief An 8-bit enum to describe how the samples are encoded.
   *
   * See `format_flags` in MP4-PCM.
   */
  enum LpcmFormatFlagsBitmask : uint8_t {
    kLpcmBigEndian = 0x00,
    kLpcmLittleEndian = 0x01,
    kLpcmBeginReserved = 0x02,
    kLpcmEndReserved = 0xff,
  };

  friend bool operator==(const LpcmDecoderConfig& lhs,
                         const LpcmDecoderConfig& rhs) = default;

  /*!\brief Returns the required audio roll distance.
   *
   * \return Audio roll distance required by the IAMF spec.
   */
  static int16_t GetRequiredAudioRollDistance() { return 0; }

  /*!\brief Returns true if the samples are encoded in little-endian format.*/
  bool IsLittleEndian() const;

  /*!\brief Validates the values in `LpcmDecoderConfig`.
   *
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status Validate() const;

  /*!\brief Validates and writes the `LpcmDecoderConfig` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the `LpcmDecoderConfig` to a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  /*!\brief Gets the output sample rate represented within the decoder config.
   *
   * This sample rate is used for timing and offset calculations.
   *
   * IAMF v1.1.0 section 3.11.4 specifies:
   *  > "The sample rate used for computing offsets SHALL be sample_rate."
   *
   * \param output_sample_rate Output sample rate.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if there is an unexpected sample rate.
   */
  absl::Status GetOutputSampleRate(uint32_t& output_sample_rate) const;

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth the user should decode the signal
   * to.
   *
   * \param bit_depth_to_measure_loudness Bit-depth of the PCM which will be
   *        used to measure loudness.
   * \return `absl::OkStatus()` if successful.  `absl::InvalidArgumentError()`
   *         if there is an unexpected bit-depth.
   */
  absl::Status GetBitDepthToMeasureLoudness(
      uint8_t& bit_depth_to_measure_loudness) const;

  /*!\brief Prints logging information about the decoder config.
   */
  void Print() const;

  LpcmFormatFlagsBitmask sample_format_flags_bitmask_;
  uint8_t sample_size_;
  uint32_t sample_rate_;
};

}  // namespace iamf_tools

#endif  // OBU_DECODER_CONFIG_LPCM_DECODER_CONFIG_H_
