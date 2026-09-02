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
#ifndef CLI_CODEC_AAC_ENCODER_H_
#define CLI_CODEC_AAC_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>

// This symbol conflicts with a macro in fdk_aac.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "libAACenc/include/aacenc_lib.h"

namespace iamf_tools {

class AacEncoder : public EncoderBase {
 public:
  /*!\brief Constructor.
   *
   * \param aac_encoder_metadata Encoder metadata for AAC.
   * \param codec_config Codec config for AAC.
   * \param channel_count Number of channels in the substream.
   */
  AacEncoder(
      const iamf_tools_cli_proto::AacEncoderMetadata& aac_encoder_metadata,
      const CodecConfigObu& codec_config, SubstreamChannelCount channel_count)
      : EncoderBase(codec_config, channel_count),
        encoder_metadata_(aac_encoder_metadata),
        decoder_config_(std::get<AacDecoderConfig>(
            codec_config.GetCodecConfig().decoder_config)) {}

  ~AacEncoder() override;

 private:
  /*!\brief Initializes the underlying encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitializeEncoder() override;

  /*!\brief Initializes `required_samples_to_delay_at_start_`.
   *
   * `InitializeEncoder` is required to be called before calling this function.
   *
   * \param validate_codec_delay Ignored.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status SetNumberOfSamplesToDelayAtStart(
      bool /*validate_codec_delay*/) override;

  /*!\brief Encodes an audio frame.
   *
   * \param samples Samples arranged in (channel, time) axes. The samples are
   *        left-justified and stored in the upper `input_bit_depth` bits.
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *        The underlying `audio_frame_` is modified. All other fields are
   *        blindly passed along.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EncodeAudioFrame(
      const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data)
      override;

  const iamf_tools_cli_proto::AacEncoderMetadata encoder_metadata_;
  const AacDecoderConfig decoder_config_;

  // A pointer to the `fdk_aac` encoder.
  AACENCODER* encoder_ = nullptr;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_AAC_ENCODER_H_
