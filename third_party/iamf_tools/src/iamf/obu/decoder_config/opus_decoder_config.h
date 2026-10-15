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
#ifndef OBU_DECODER_CONFIG_OPUS_DECODER_CONFIG_H_
#define OBU_DECODER_CONFIG_OPUS_DECODER_CONFIG_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief The `CodecConfig` `decoder_config` field for Opus.*/
class OpusDecoderConfig {
 public:
  static constexpr uint8_t kOutputChannelCount = 2;
  static constexpr int16_t kOutputGain = 0;
  static constexpr uint8_t kMappingFamily = 0;

  friend bool operator==(const OpusDecoderConfig& lhs,
                         const OpusDecoderConfig& rhs) = default;

  /*!\brief Returns the required audio roll distance.
   *
   * \param num_samples_per_frame `num_samples_per_frame` in the associated
   *        Codec Config OBU.
   * \return Required audio roll distance, or an error if there would be a
   *         divide by zero.
   */
  static absl::StatusOr<int16_t> GetRequiredAudioRollDistance(
      uint32_t num_samples_per_frame);

  /*!\brief Validates and writes the `OpusDecoderConfig` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Validates and reads the `OpusDecoderConfig` from a buffer.
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
   * IAMF v1.1.0 section 3.11.1 specifies:
   *  > "The sample rate used for computing offsets SHALL be 48 kHz."
   *
   * \return Output sample rate.
   */
  uint32_t GetOutputSampleRate() const { return 48000; }

  /*!\brief Gets the input sample rate represented within the decoder config.
   *
   * Opus explicitly has this value in the Codec Config OBU.
   *
   * \return Input sample rate.
   */
  uint32_t GetInputSampleRate() const { return input_sample_rate_; }

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth associated substreams should be
   * decoded to. The encoder provides data to `libopus::opus_encoder_float()` as
   * a `float` in the range [-1, +1].
   *
   * \return Bit-depth of the PCM which will be used to measure loudness if the
   *         OBU was initialized successfully.
   */
  static constexpr uint8_t GetBitDepthToMeasureLoudness() { return 32; }

  /*!\brief Prints logging information about the decoder config.
   */
  void Print() const;

  uint8_t version_;
  uint16_t pre_skip_;
  uint32_t input_sample_rate_;
  int16_t output_gain_ = kOutputGain;
  uint8_t mapping_family_ = kMappingFamily;
};

}  // namespace iamf_tools

#endif  // OBU_DECODER_CONFIG_OPUS_DECODER_CONFIG_H_
