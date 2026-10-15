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
#include "iamf/cli/audio_frame_decoder.h"

#include <memory>
#include <utility>
#include <variant>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/lpcm_decoder.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"

// These defines are not part of an official API and are likely to change or be
// removed.  Please do not depend on them.
// TODO(b/401063570): Remove these statements when no longer disabling FLAC/AAC.
#ifndef IAMF_TOOLS_DISABLE_AAC_DECODER
#include "iamf/cli/codec/aac_decoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_FLAC_DECODER
#include "iamf/cli/codec/flac_decoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_OPUS_DECODER
#include "iamf/cli/codec/opus_decoder.h"
#endif

namespace iamf_tools {

namespace {

absl::StatusOr<std::unique_ptr<DecoderBase>> CreateDecoder(
    const CodecConfigObu& codec_config, SubstreamChannelCount channel_count) {
  const auto* decoder_config = &codec_config.GetCodecConfig().decoder_config;
  switch (codec_config.GetCodecConfig().GetCodecId()) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm: {
      auto* lpcm_decoder_config =
          std::get_if<LpcmDecoderConfig>(decoder_config);
      if (lpcm_decoder_config == nullptr) {
        return absl::InvalidArgumentError(
            "CodecConfigObu does not contain an `LpcmDecoderConfig`.");
      }
      return LpcmDecoder::Create(*lpcm_decoder_config, channel_count,
                                 codec_config.GetNumSamplesPerFrame());
    }
#ifndef IAMF_TOOLS_DISABLE_OPUS_DECODER
    case kCodecIdOpus: {
      auto* opus_decoder_config =
          std::get_if<OpusDecoderConfig>(decoder_config);
      if (opus_decoder_config == nullptr) {
        return absl::InvalidArgumentError(
            "CodecConfigObu does not contain an `OpusDecoderConfig`.");
      }
      return OpusDecoder::Create(*opus_decoder_config, channel_count,
                                 codec_config.GetNumSamplesPerFrame());
    }
#endif
#ifndef IAMF_TOOLS_DISABLE_AAC_DECODER
    case kCodecIdAacLc: {
      auto* aac_decoder_config = std::get_if<AacDecoderConfig>(decoder_config);
      if (aac_decoder_config == nullptr) {
        return absl::InvalidArgumentError(
            "CodecConfigObu does not contain an `AacDecoderConfig`.");
      }
      return AacDecoder::Create(*aac_decoder_config, channel_count,
                                codec_config.GetNumSamplesPerFrame());
    }
#endif
#ifndef IAMF_TOOLS_DISABLE_FLAC_DECODER
    case kCodecIdFlac:
      return FlacDecoder::Create(channel_count,
                                 codec_config.GetNumSamplesPerFrame());
#endif
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unrecognized or disabled codec_id= ",
                       codec_config.GetCodecConfig().GetCodecId()));
  }
}

}  // namespace

// Initializes all decoders based on the corresponding Audio Element and Codec
// Config OBUs.
absl::Status AudioFrameDecoder::InitDecodersForSubstreams(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const CodecConfigObu& codec_config) {
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    auto& decoder_for_substream = substream_id_to_decoder_[substream_id];
    if (decoder_for_substream != nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Already initialized decoder for substream ID: ", substream_id,
          ". Maybe multiple Audio Element OBUs have the same substream ID?"));
    }

    auto channel_count = SubstreamChannelCount::Create(labels.size());
    if (!channel_count.ok()) {
      return channel_count.status();
    }

    // Create the decoder, unwrap it and move it into the map, if it is valid.
    absl::StatusOr<std::unique_ptr<DecoderBase>> new_decoder =
        CreateDecoder(codec_config, *channel_count);
    if (!new_decoder.ok()) {
      return new_decoder.status();
    }
    // The factories are not supposed to return an OK and `nullptr` decoder.
    // For defensive programming, check for that case.
    ABSL_CHECK_NE(*new_decoder, nullptr);
    decoder_for_substream = std::move(*new_decoder);
  }

  return absl::OkStatus();
}

absl::Status AudioFrameDecoder::Decode(AudioFrameWithData& audio_frame) {
  auto decoder_iter =
      substream_id_to_decoder_.find(audio_frame.obu.GetSubstreamId());
  if (decoder_iter == substream_id_to_decoder_.end() ||
      decoder_iter->second == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("No decoder found for substream ID: ",
                     audio_frame.obu.GetSubstreamId()));
  }
  // Decode the samples with the specific decoder associated with this
  // substream.
  auto& decoder = *decoder_iter->second;
  RETURN_IF_NOT_OK(decoder.DecodeAudioFrame(
      absl::MakeConstSpan(audio_frame.obu.audio_frame_)));

  // Fill in the decoded samples.
  audio_frame.decoded_samples = decoder.ValidDecodedSamples();
  return absl::OkStatus();
}

}  // namespace iamf_tools
