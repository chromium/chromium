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
#include "iamf/cli/codec/opus_encoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/opus_utils.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "include/opus.h"
#include "include/opus_defines.h"
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
    auto error_message = absl::StrCat(
        "IAMF v1.1.0 expects output_gain: ", opus_decoder_config.output_gain_,
        " and mapping_family: ", opus_decoder_config.mapping_family_,
        " to be 0.");
    return absl::InvalidArgumentError(error_message);
  }

  return absl::OkStatus();
}

// `opus_encode_float` recommends the input is normalized to the range [-1, 1].
const absl::AnyInvocable<absl::Status(int32_t, float&) const>
    kInt32ToNormalizedFloat = [](int32_t input, float& output) {
      output = Int32ToNormalizedFloatingPoint<float>(input);
      return absl::OkStatus();
    };

absl::StatusOr<int> EncodeFloat(
    const std::vector<std::vector<int32_t>>& samples,
    int num_samples_per_channel, ::OpusEncoder* encoder,
    std::vector<uint8_t>& audio_frame) {
  // TODO(b/382197581): Avoid re-allocations of `encoder_input_pcm` and
  //                    `samples_spans`.
  std::vector<float> encoder_input_pcm;
  std::vector<absl::Span<const int32_t>> samples_spans(samples.size());
  for (size_t c = 0; c < samples.size(); c++) {
    samples_spans[c] = absl::MakeConstSpan(samples[c]);
  }

  RETURN_IF_NOT_OK(ConvertChannelTimeToInterleaved(
      absl::MakeConstSpan(samples_spans), encoder_input_pcm,
      kInt32ToNormalizedFloat));

  // TODO(b/311655037): Test that samples are passed to `opus_encode_float` in
  //                    the correct order. Maybe also check they are in the
  //                    correct [-1, +1] range. This may requiring mocking a
  //                    simple version of `opus_encode_float`.
  return opus_encode_float(encoder, encoder_input_pcm.data(),
                           num_samples_per_channel, audio_frame.data(),
                           static_cast<opus_int32>(audio_frame.size()));
}

absl::StatusOr<int> EncodeInt16(
    const std::vector<std::vector<int32_t>>& samples,
    int num_samples_per_channel, SubstreamChannelCount channel_count,
    ::OpusEncoder* encoder, std::vector<uint8_t>& audio_frame) {
  // `libopus` requires the native system endianness as input.
  const bool big_endian = IsNativeBigEndian();

  // TODO(b/382197581): Avoid re-allocations of `encoder_input_pcm`.
  // Convert input to the array that will be passed to `opus_encode`.
  std::vector<opus_int16> encoder_input_pcm(
      num_samples_per_channel * channel_count.num_channels(), 0);
  size_t write_position = 0;
  for (size_t t = 0; t < samples[0].size(); t++) {
    for (size_t c = 0; c < samples.size(); c++) {
      // Convert all frames to 16-bit samples for input to Opus.
      // Write the 16-bit samples directly into the pcm vector.
      RETURN_IF_NOT_OK(
          WritePcmSample(static_cast<uint32_t>(samples[c][t]), 16, big_endian,
                         reinterpret_cast<uint8_t*>(encoder_input_pcm.data()),
                         write_position));
    }
  }

  return opus_encode(encoder, encoder_input_pcm.data(), num_samples_per_channel,
                     audio_frame.data(),
                     static_cast<opus_int32>(audio_frame.size()));
}

}  // namespace

absl::Status OpusEncoder::SetNumberOfSamplesToDelayAtStart(
    bool validate_codec_delay) {
  opus_int32 lookahead;
  int opus_error_code =
      opus_encoder_ctl(encoder_, OPUS_GET_LOOKAHEAD(&lookahead));
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(opus_error_code,
                                             "Failed to get Opus lookahead."));
  ABSL_LOG_FIRST_N(INFO, 1) << "Opus lookahead=" << lookahead;
  // Opus calls the number of samples that should be trimmed/pre-skipped
  // `lookahead`.
  required_samples_to_delay_at_start_ = static_cast<uint32_t>(lookahead);
  if (validate_codec_delay) {
    MAYBE_RETURN_IF_NOT_OK(
        ValidateEqual(static_cast<uint32_t>(decoder_config_.pre_skip_),
                      required_samples_to_delay_at_start_, "Opus `pre_skip`"));
  }

  return absl::OkStatus();
}

absl::Status OpusEncoder::InitializeEncoder() {
  MAYBE_RETURN_IF_NOT_OK(ValidateDecoderConfig(decoder_config_));

  int opus_error_code;
  encoder_ =
      opus_encoder_create(input_sample_rate_, channel_count_.num_channels(),
                          settings_.libopus_application_mode, &opus_error_code);
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(
      opus_error_code, "Failed to initialize Opus encoder."));

  opus_error_code =
      opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(static_cast<opus_int32>(
                                     settings_.target_substream_bitrate)));
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(opus_error_code,
                                             "Failed to set Opus bitrate."));

  return absl::OkStatus();
}

OpusEncoder::~OpusEncoder() { opus_encoder_destroy(encoder_); }

absl::Status OpusEncoder::EncodeAudioFrame(
    const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateNotFinalized());
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  // Opus output could take up to 4 bytes per sample. Reserve an output vector
  // of the maximum possible size.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(
      num_samples_per_channel * channel_count_.num_channels() * 4, 0);

  const auto encoded_length_bytes =
      settings_.use_float_api
          ? EncodeFloat(samples, num_samples_per_channel, encoder_, audio_frame)
          : EncodeInt16(samples, num_samples_per_channel, channel_count_,
                        encoder_, audio_frame);

  if (!encoded_length_bytes.ok()) {
    return encoded_length_bytes.status();
  }

  if (*encoded_length_bytes < 0) {
    // When `encoded_length_bytes` is negative, it is a non-OK Opus error code.
    return OpusErrorCodeToAbslStatus(*encoded_length_bytes,
                                     "Failed to encode samples.");
  }

  // Shrink output vector to actual size.
  audio_frame.resize(*encoded_length_bytes);

  absl::MutexLock lock(mutex_);
  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  return absl::OkStatus();
}

}  // namespace iamf_tools
