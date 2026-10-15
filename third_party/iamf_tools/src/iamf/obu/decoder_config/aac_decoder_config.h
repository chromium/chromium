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
#ifndef OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_
#define OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief `AudioSpecificConfig` as defined in ISO 14496-3. */
class AudioSpecificConfig {
 public:
  /*!\brief A 4-bit enum to describe the sampling frequency.
   *
   * See `extensionSamplingFrequencyIndex` in ISO-14496.
   */
  enum class SampleFrequencyIndex {
    k96000 = 0,
    k88200 = 1,
    k64000 = 2,
    k48000 = 3,
    k44100 = 4,
    k32000 = 5,
    k24000 = 6,
    k22050 = 7,
    k16000 = 8,
    k12000 = 9,
    k11025 = 10,
    k8000 = 11,
    k7350 = 12,
    kReservedA = 13,
    kReservedB = 14,
    // IAMF forbids the use of the escape value (15).
  };

  static constexpr uint8_t kAudioObjectType = 2;
  static constexpr uint8_t kChannelConfiguration = 2;

  friend bool operator==(const AudioSpecificConfig& lhs,
                         const AudioSpecificConfig& rhs) = default;

  /*!\brief Validates and writes the `AudioSpecificConfig` to a buffer.
   *
   * \return `absl::OkStatus()` if the `AudioSpecificConfig` is valid. A
   *         specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads the `AudioSpecificConfig` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` on success. A specific error code on failure.
   */
  absl::Status Read(ReadBitBuffer& rb);

  /*!\brief Prints logging information about the audio specific config.
   */
  void Print() const;

  uint8_t audio_object_type_ = kAudioObjectType;  // 5 bits.
  SampleFrequencyIndex sample_frequency_index_;   // 4 bits.
  // IAMF forbids the use of the escape value (15) for
  // `sample_frequency_index_`.
  uint8_t channel_configuration_ = kChannelConfiguration;  // 4 bits.

  // The ISO spec allows several different types of configs to follow depending
  // on `audio_object_type`. Valid IAMF streams always use the general audio
  // specific config because of the fixed `audio_object_type == 2`.
  struct GaSpecificConfig {
    static constexpr bool kFrameLengthFlag = false;
    static constexpr bool kDependsOnCoreCoder = false;
    static constexpr bool kExtensionFlag = false;

    friend bool operator==(const GaSpecificConfig& lhs,
                           const GaSpecificConfig& rhs) = default;

    bool frame_length_flag = kFrameLengthFlag;
    bool depends_on_core_coder = kDependsOnCoreCoder;
    bool extension_flag = kExtensionFlag;
  } ga_specific_config_;
};

/*!\brief The `CodecConfig` `decoder_config` field for AAC.
 *
 * As defined in IAMF v1.1.0 section 3.11.2
 * https://aomediacodec.github.io/iamf/#aac-lc-specific. Many fields are fixed
 * by the IAMF spec and should typically never be changed from their default
 * values.
 */
class AacDecoderConfig {
 public:
  static constexpr uint8_t kDecoderConfigDescriptorTag = 0x04;
  static constexpr uint8_t kObjectTypeIndication = 0x40;
  static constexpr uint8_t kStreamType = 0x05;
  static constexpr bool kUpstream = false;
  static constexpr bool kReserved = true;
  static constexpr auto kSampleFrequencyIndexAndSampleFrequency = []() {
    using enum AudioSpecificConfig::SampleFrequencyIndex;
    return std::to_array<
        std::pair<AudioSpecificConfig::SampleFrequencyIndex, uint32_t>>(
        {{k96000, 96000},
         {k88200, 88200},
         {k64000, 64000},
         {k48000, 48000},
         {k44100, 44100},
         {k32000, 32000},
         {k24000, 24000},
         {k22050, 22050},
         {k16000, 16000},
         {k12000, 12000},
         {k11025, 11025},
         {k8000, 8000},
         {k7350, 7350}});
  }();

  friend bool operator==(const AacDecoderConfig& lhs,
                         const AacDecoderConfig& rhs) = default;

  /*!\brief Returns the required audio roll distance.
   *
   * \return Audio roll distance required by the IAMF spec.
   */
  static int16_t GetRequiredAudioRollDistance() { return -1; }

  /*!\brief Validates the `AacDecoderConfig`.
   *
   * \return `absl::OkStatus()` if the decoder config is valid. A specific
   *         status on failure.
   */
  absl::Status Validate() const;

  /*!\brief Validates and writes the `AacDecoderConfig` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific
   *         status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Validates and reads the `AacDecoderConfig` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific error
   *         code on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb);

  /*!\brief Gets the output sample rate of the `AacDecoderConfig`.
   *
   * This sample rate is used for timing and offset calculations.
   *
   * IAMF v1.1.0 section 3.11.2 specifies:
   *  > "The sample rate used for computing offsets SHALL be the rate indicated
   *     by the samplingFrequencyIndex in GASpecificConfig()."
   *
   * \param output_sample_rate Output sample rate.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if the metadata is an unrecognized type.
   */
  absl::Status GetOutputSampleRate(uint32_t& output_sample_rate) const;

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth associated substreams should be
   * decoded to.
   *
   * \return Bit-depth of the PCM which will be used to measure loudness if the
   *         OBU was initialized successfully.
   */
  static uint8_t GetBitDepthToMeasureLoudness();

  /*!\brief Prints logging information about the decoder config.
   */
  void Print() const;

  uint8_t decoder_config_descriptor_tag_ = kDecoderConfigDescriptorTag;
  // ISO 14496-1 8.3.3 expandable field is inserted automatically.
  uint8_t object_type_indication_ = kObjectTypeIndication;
  uint8_t stream_type_ = kStreamType;  // 6 bits.
  bool upstream_ = kUpstream;
  bool reserved_ = kReserved;
  uint32_t buffer_size_db_;  // 24 bits.
  uint32_t max_bitrate_;
  uint32_t average_bit_rate_;

  struct DecoderSpecificInfo {
    static constexpr uint8_t kDecoderSpecificInfoTag = 0x05;

    friend bool operator==(const DecoderSpecificInfo& lhs,
                           const DecoderSpecificInfo& rhs) = default;

    uint8_t decoder_specific_info_tag = kDecoderSpecificInfoTag;
    // ISO 14496-1 8.3.3 expandable field is inserted automatically.
    AudioSpecificConfig audio_specific_config;
    std::vector<uint8_t> decoder_specific_info_extension;
  } decoder_specific_info_;

  std::vector<uint8_t> decoder_config_extension_;
  // ProfileLevelIndicationIndexDescriptor is an extension in the original
  // message, but is unused in IAMF.
};

}  // namespace iamf_tools

#endif  // OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_
