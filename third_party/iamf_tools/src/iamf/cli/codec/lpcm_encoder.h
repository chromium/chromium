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
#ifndef CLI_CODEC_LPCM_ENCODER_H_
#define CLI_CODEC_LPCM_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"

namespace iamf_tools {

class LpcmEncoder : public EncoderBase {
 public:
  /*!\brief Constructor.
   *
   * \param codec_config Codec config for LPCM.
   * \param channel_count Number of channels in the substream.
   */
  LpcmEncoder(const CodecConfigObu& codec_config,
              SubstreamChannelCount channel_count)
      : EncoderBase(codec_config, channel_count),
        decoder_config_(std::get<LpcmDecoderConfig>(
            codec_config.GetCodecConfig().decoder_config)) {}

  ~LpcmEncoder() override = default;

 private:
  /*!\brief Initializes the underlying encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitializeEncoder() override;

  /*!\brief Encodes an audio frame.
   *
   * \param samples Samples arranged in (channel, time) axes. The samples are
   *        left-justified and stored in the upper `input_bit_depth` bits.
   *
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *        The underlying `audio_frame_` is modified. All other fields are
   *        blindly passed along.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EncodeAudioFrame(
      const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data)
      override;

  const LpcmDecoderConfig decoder_config_;
};
}  // namespace iamf_tools

#endif  // CLI_CODEC_LPCM_ENCODER_H_
