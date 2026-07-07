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
#include "iamf/cli/codec/opus_decoder.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/opus_utils.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "iamf/obu/types.h"
#include "include/opus.h"
#include "include/opus_types.h"

namespace iamf_tools {

namespace {

// Performs validation for values that this implementation assumes are
// restricted because they are restricted in IAMF v1.1.0.
absl::Status ValidateDecoderConfig(
    const OpusDecoderConfig& opus_decoder_config) {
  // Validate the input. Reject values that would need to be added to this
  // function if they were ever supported.
  if (opus_decoder_config.output_gain_ != 0 ||
      opus_decoder_config.mapping_family_ != 0) {
    const auto error_message = absl::StrCat(
        "IAMF v1.1.0 expects output_gain: ", opus_decoder_config.output_gain_,
        " and mapping_family: ", opus_decoder_config.mapping_family_,
        " to be 0.");
    return absl::InvalidArgumentError(error_message);
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<DecoderBase>> OpusDecoder::Create(
    const OpusDecoderConfig& decoder_config,
    SubstreamChannelCount channel_count, uint32_t num_samples_per_frame) {
  MAYBE_RETURN_IF_NOT_OK(ValidateDecoderConfig(decoder_config));

  // Initialize the decoder.
  int opus_error_code;
  LibOpusDecoder* decoder = opus_decoder_create(
      static_cast<opus_int32>(decoder_config.GetOutputSampleRate()),
      channel_count.num_channels(), &opus_error_code);
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(
      opus_error_code, "Failed to initialize Opus decoder."));
  if (decoder == nullptr) {
    return absl::UnknownError("Unexpected null decoder after initialization.");
  }

  return absl::WrapUnique(
      new OpusDecoder(channel_count, num_samples_per_frame, decoder));
}

OpusDecoder::~OpusDecoder() {
  // The factory function prevents `decoder_` from ever being null.
  ABSL_CHECK_NE(decoder_, nullptr);
  opus_decoder_destroy(decoder_);
}

absl::Status OpusDecoder::DecodeAudioFrame(
    absl::Span<const uint8_t> encoded_frame) {
  // `opus_decode_float` decodes to `float` samples with channels interlaced.
  // Typically these values are in the range of [-1, +1] (always for
  // `iamf_tools`-encoded data). Values outside of that range will be clipped in
  // `NormalizedFloatingPointToInt32`.
  const int num_output_samples = opus_decode_float(
      decoder_, reinterpret_cast<const unsigned char*>(encoded_frame.data()),
      static_cast<opus_int32>(encoded_frame.size()),
      interleaved_float_from_libopus_.data(),
      /*frame_size=*/num_samples_per_channel_,
      /*decode_fec=*/0);
  if (num_output_samples < 0) {
    // When `num_output_samples` is negative, it is a non-OK Opus error code.
    return OpusErrorCodeToAbslStatus(num_output_samples,
                                     "Failed to decode Opus frame.");
  }
  ABSL_LOG_FIRST_N(INFO, 1)
      << "Opus decoded " << num_output_samples << " samples per channel. With "
      << channel_count_.num_channels() << " channels.";

  // Convert the interleaved data to (channel, time) axes
  const absl::AnyInvocable<absl::Status(float, InternalSampleType&) const>
      kFloatToInternalSampleType = [](float input, InternalSampleType& output) {
        output = static_cast<InternalSampleType>(input);
        return absl::OkStatus();
      };
  return ConvertInterleavedToChannelTime(
      absl::MakeConstSpan(interleaved_float_from_libopus_)
          .first(num_output_samples * channel_count_.num_channels()),
      channel_count_.num_channels(), decoded_samples_,
      kFloatToInternalSampleType);
}

}  // namespace iamf_tools
