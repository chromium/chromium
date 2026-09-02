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
#ifndef CLI_CODEC_OPUS_ENCODER_H_
#define CLI_CODEC_OPUS_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "include/opus.h"
#include "include/opus_defines.h"

namespace iamf_tools {

class OpusEncoder : public EncoderBase {
 public:
  struct Settings {
    // Controls whether to use the `libopus` float API or int16_t API.
    bool use_float_api = true;
    // The `libopus` application mode, usually set to `OPUS_APPLICATION_AUDIO`,
    // `OPUS_APPLICATION_VOIP`, or `OPUS_APPLICATION_RESTRICTED_LOWDELAY`.
    int libopus_application_mode = OPUS_APPLICATION_AUDIO;
    // The total target bitrate for the substream. I.e. in a coupled substream
    // this bitrate is shared among two channels. Typically this in bits per
    // second. Sentinel values defined by `libopus` are permitted, including
    // `OPUS_BITRATE_MAX` and `OPUS_AUTO`.
    int32_t target_substream_bitrate = OPUS_AUTO;
  };

  /*!\brief Constructor.
   *
   * \param settings Encoder settings.
   * \param codec_config Codec config OBU.
   * \param channel_count Number of substream channels in the encoded audio.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  OpusEncoder(const Settings& settings, const CodecConfigObu& codec_config,
              SubstreamChannelCount channel_count)
      : EncoderBase(codec_config, channel_count),
        settings_(settings),
        decoder_config_(std::get<OpusDecoderConfig>(
            codec_config.GetCodecConfig().decoder_config)) {}

  ~OpusEncoder() override;

 private:
  // The encoder from `libopus` is in the global namespace.
  typedef ::OpusEncoder LibOpusEncoder;

  /*!\brief Initializes the underlying encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitializeEncoder() override;

  /*!\brief Initializes `required_samples_to_delay_at_start_`.
   *
   * `InitializeEncoder` is required to be called before calling this function.
   * This value may vary based on `encoder_metadata_`, num_channels_` or
   * settings in the associated Codec Config OBU.
   *
   * \param validate_codec_delay If true, validates `pre_skip` agrees with the
   *        encoder.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status SetNumberOfSamplesToDelayAtStart(
      bool validate_codec_delay) override;

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

  /*!\brief Validates the underlying encoder.
   *
   * Configures the encoder based on the `encoder_metadata_`, the associated
   * Codec Config OBU, and IAMF requirements.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateEncoderInfo();

  const Settings settings_;
  const OpusDecoderConfig decoder_config_;

  LibOpusEncoder* encoder_ = nullptr;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_OPUS_ENCODER_H_
