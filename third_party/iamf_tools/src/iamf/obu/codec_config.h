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
#ifndef OBU_CODEC_CONFIG_H_
#define OBU_CODEC_CONFIG_H_

#include <cstdint>
#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

// TODO(b/305752871): Port this `std::variant` to a virtual class.
typedef std::variant<OpusDecoderConfig, AacDecoderConfig, FlacDecoderConfig,
                     LpcmDecoderConfig>
    DecoderConfig;

struct CodecConfig {
  enum CodecId : uint32_t {
    kCodecIdOpus = 0x4f707573,   // "Opus"
    kCodecIdFlac = 0x664c6143,   // "fLaC"
    kCodecIdLpcm = 0x6970636d,   // "ipcm"
    kCodecIdAacLc = 0x6d703461,  // "mp4a"
  };

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const CodecId& id) {
    std::string human_readable_codec_id;
    switch (id) {
      case kCodecIdOpus:
        human_readable_codec_id = "Opus";
        break;
      case kCodecIdFlac:
        human_readable_codec_id = "FLAC";
        break;
      case kCodecIdLpcm:
        human_readable_codec_id = "LPCM";
        break;
      case kCodecIdAacLc:
        human_readable_codec_id = "AAC LC";
        break;
      default:
        human_readable_codec_id = "Unknown";
        break;
    }

    sink.Append(absl::StrCat("0x", absl::Hex(id, absl::kZeroPad8), " ( ",
                             human_readable_codec_id, " )"));
  }

  /*!\brief Gets the codec ID inferred from the `decoder_config`.
   *
   * \return Codec ID of the underlying `decoder_config`.
   */
  CodecId GetCodecId() const;

  /*!\brief Gets the audio roll distance of the codec config.
   *
   * \return Audio roll distance based on the underlying `decoder_config`.
   */
  absl::StatusOr<int16_t> GetAudioRollDistance() const;

  friend bool operator==(const CodecConfig& lhs,
                         const CodecConfig& rhs) = default;

  DecodedUleb128 num_samples_per_frame;

  DecoderConfig decoder_config;
};

/*!\brief Codec Config OBU based on section 3.5 of the IAMF specification.
 *
 * This class has stricter limits than the specification:
 *   - Number of samples per frame is limited to `kMaxPracticalFrameSize`.
 */
class CodecConfigObu : public ObuBase {
 public:
  /*!\brief Artificial limit on the maximum number of samples per frame. */
  static constexpr uint32_t kMaxPracticalFrameSize = 96000;

  /*!\brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param codec_config_id `codec_config_id` in the OBU.
   * \param input_codec_config `codec_config` in the OBU.
   * \return `CodecConfigObu` on success. A specific status on failure.
   */
  static absl::StatusOr<CodecConfigObu> Create(
      const ObuHeader& header, DecodedUleb128 codec_config_id,
      const CodecConfig& input_codec_config);

  /*!\brief Creates a `CodecConfigObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `CodecConfigObu` data is stored. Data
   *        read from the buffer is consumed.
   * \return a `CodecConfigObu` on success. A specific status on failure.
   */
  static absl::StatusOr<CodecConfigObu> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb);

  /*!\brief Destructor. */
  ~CodecConfigObu() override = default;

  friend bool operator==(const CodecConfigObu& lhs,
                         const CodecConfigObu& rhs) = default;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*!\brief Sets the codec delay in the underlying `decoder_config`.
   *
   * In some codecs, like Opus, the codec delay is called "pre-skip". This may
   * be a no-op when the underlying `decoder_config` does not have a codec delay
   * field.
   *
   * \param codec_delay Codec delay to set in the underlying `decoder_config`.
   */
  void SetCodecDelay(uint16_t codec_delay);

  /*!\brief Gets the output sample rate associated with the OBU.
   *
   * This sample rate is used for timing and offset calculations as per
   * section 3.11 of the IAMF spec.
   *
   *   - AAC, FLAC, LPCM: Based on the sample rate of the input stream.
   *   - Opus: Always 48kHz ("The sample rate used for computing offsets SHALL
   *           be 48 kHz.").
   *
   * \return Output sample rate in Hz.
   */
  uint32_t GetOutputSampleRate() const { return output_sample_rate_; }

  /*!\brief Gets the input sample rate associated with the OBU.
   *
   * The sample rate of the data before being passed to the underlying codec
   * libraries.
   *
   * \return Input sample rate in Hz.
   */
  uint32_t GetInputSampleRate() const { return input_sample_rate_; }

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth associated substreams should be
   * decoded to.
   *
   * \return Bit-depth of the PCM which will be used to measure loudness.
   */
  uint32_t GetBitDepthToMeasureLoudness() const {
    return bit_depth_to_measure_loudness_;
  }

  /*!\brief Gets the number of samples per frame of the OBU.
   *
   * \return Num samples per frame of the OBU.
   */
  uint32_t GetNumSamplesPerFrame() const {
    return codec_config_.num_samples_per_frame;
  }

  /*!\brief Gets the codec config id of the OBU.
   *
   * \return codec config id of the OBU.
   */
  DecodedUleb128 GetCodecConfigId() const { return codec_config_id_; }

  /*!\brief Gets the codec config of the OBU.
   *
   * \return codec config of the OBU.
   */
  const CodecConfig& GetCodecConfig() const { return codec_config_; }

  bool IsLossless() const;

 private:
  /*!\brief Used only by the factory `Create()` function.
   *
   * \param header `ObuHeader` of the OBU.
   * \param codec_config_id `codec_config_id` in the OBU.
   * \param codec_config `codec_config` in the OBU.
   * \param output_sample_rate Output sample rate in Hz.
   * \param input_sample_rate Input sample rate in Hz.
   * \param bit_depth_to_measure_loudness Bit-depth of the PCM which will be
   *        used to measure loudness.
   */
  CodecConfigObu(const ObuHeader& header, DecodedUleb128 codec_config_id,
                 const CodecConfig& codec_config, uint32_t output_sample_rate,
                 uint32_t input_sample_rate,
                 uint8_t bit_depth_to_measure_loudness);

  // Used only by the factory `CreateFromBuffer` function.
  explicit CodecConfigObu(const ObuHeader& header)
      : CodecConfigObu(header, /*codec_config_id=*/0, CodecConfig(),
                       /*output_sample_rate=*/0, /*input_sample_rate=*/0,
                       /*bit_depth_to_measure_loudness=*/0) {}

  // Fields in the OBU as per the IAMF specification.
  DecodedUleb128 codec_config_id_;
  CodecConfig codec_config_;

  // Metadata fields.
  uint32_t input_sample_rate_ = 0;
  uint32_t output_sample_rate_ = 0;
  uint8_t bit_depth_to_measure_loudness_ = 0;

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *         failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // OBU_CODEC_CONFIG_H_
