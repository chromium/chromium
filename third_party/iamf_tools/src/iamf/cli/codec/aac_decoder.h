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
#ifndef CLI_CODEC_AAC_DECODER_H_
#define CLI_CODEC_AAC_DECODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"

// This symbol conflicts with a macro in fdk_aac.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "libAACdec/include/aacdecoder_lib.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

// TODO(b/277731089): Test sample accuracy of `DecodeAudioFrame`.
class AacDecoder : public DecoderBase {
 public:
  /*!\brief Factory function.
   *
   * \param decoder_config Decoder config for this stream.
   * \param channel_count Channel count for this substream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \return AAC decoder on success. A specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<DecoderBase>> Create(
      const AacDecoderConfig& decoder_config,
      SubstreamChannelCount channel_count, uint32_t num_samples_per_frame);

  /*!\brief Destructor.
   */
  ~AacDecoder() override;

  /*!\brief Decodes an AAC audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      absl::Span<const uint8_t> encoded_frame) override;

 private:
  /* Private constructor.
   *
   * Used only by the factory function.
   *
   * \param channel_count Number of channels for this substream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \param decoder `fdk_aac` decoder to use.
   */
  AacDecoder(SubstreamChannelCount channel_count,
             uint32_t num_samples_per_frame,
             AAC_DECODER_INSTANCE* absl_nonnull decoder)
      : DecoderBase(channel_count, num_samples_per_frame),
        interleaved_pcm_from_libfdk_aac_(num_samples_per_frame *
                                         channel_count.num_channels()),
        decoder_(decoder) {}

  // Resizes to the size of the largest input frame.
  std::vector<UCHAR> raws_frame_to_libfdk_aac_;
  // Size fixed at construction time.
  std::vector<INT_PCM> interleaved_pcm_from_libfdk_aac_;
  AAC_DECODER_INSTANCE* absl_nonnull const decoder_;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_AAC_DECODER_H_
