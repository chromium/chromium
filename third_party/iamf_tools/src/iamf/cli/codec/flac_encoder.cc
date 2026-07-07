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
#include "iamf/cli/codec/flac_encoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_encoder.h"

namespace iamf_tools {

namespace {

absl::Status FlacStreamEncoderInitStatusToAbslStatus(
    FLAC__StreamEncoderInitStatus init_status) {
  if (init_status == FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    return absl::OkStatus();
  }

  using enum absl::StatusCode;
  constexpr auto kFlacErrorAndAbslStatusCode = std::to_array<
      std::pair<FLAC__StreamEncoderInitStatus, absl::StatusCode>>({
      {FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR, kFailedPrecondition},
      {FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER, kUnimplemented},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS, kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS,
       kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE,
       kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE, kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE, kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER,
       kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION,
       kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER,
       kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE, kFailedPrecondition},
      {FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA, kInvalidArgument},
      {FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED,
       kFailedPrecondition},
  });
  static const auto kFlacErrorToAbslStatusCode =
      BuildStaticMapFromPairs(kFlacErrorAndAbslStatusCode);

  return absl::Status(LookupInMap(*kFlacErrorToAbslStatusCode, init_status,
                                  "FLAC__StreamEncoderInitStatus")
                          .value_or(kUnknown),
                      FLAC__StreamEncoderInitStatusString[init_status]);
}

absl::Status Configure(
    const iamf_tools_cli_proto::FlacEncoderMetadata& encoder_metadata,
    const FlacDecoderConfig& decoder_config,
    SubstreamChannelCount channel_count, uint32_t num_samples_per_frame,
    uint32_t output_sample_rate, uint8_t input_pcm_bit_depth_,
    FLAC__StreamEncoder* const encoder) {
  FLAC__bool ok = true;
  // Configure values based on the associated Codec Config OBU.

  ok &=
      FLAC__stream_encoder_set_channels(encoder, channel_count.num_channels());

  ok &= FLAC__stream_encoder_set_bits_per_sample(
      encoder, static_cast<uint32_t>(input_pcm_bit_depth_));

  ok &= FLAC__stream_encoder_set_sample_rate(encoder, output_sample_rate);

  // IAMF requires a constant block size.
  ok &= FLAC__stream_encoder_set_blocksize(encoder, num_samples_per_frame);

  uint64_t total_samples_in_stream;
  RETURN_IF_NOT_OK(
      decoder_config.GetTotalSamplesInStream(total_samples_in_stream));

  ok &= FLAC__stream_encoder_set_total_samples_estimate(
      encoder, total_samples_in_stream);

  // Set arguments configured by the user-provided `encoder_metadata_`.
  ok &= FLAC__stream_encoder_set_compression_level(
      encoder, encoder_metadata.compression_level());

  ok &= FLAC__stream_encoder_set_verify(encoder, true);

  if (!ok) {
    return absl::UnknownError("Failed to configure Flac encoder.");
  }

  return absl::OkStatus();
}
}  // namespace

FLAC__StreamEncoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamEncoder* /*encoder*/, const FLAC__byte buffer[],
    size_t bytes, unsigned int samples, unsigned int current_frame,
    void* client_data) {
  const unsigned int kLibFlacMetadataSentinel = 0;
  if (samples == kLibFlacMetadataSentinel) {
    // `libflac` uses a value of `0` to indicate this callback is for metadata.
    ABSL_LOG_FIRST_N(INFO, 1)
        << "`iamf_tools` currently ignores all additional FLAC metadata.";
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
  }

  auto flac_encoder = static_cast<FlacEncoder*>(client_data);

  absl::MutexLock lock(flac_encoder->mutex_);

  auto flac_frame_iter =
      flac_encoder->frame_index_to_frame_.find(current_frame);
  if (flac_frame_iter == flac_encoder->frame_index_to_frame_.end()) {
    ABSL_LOG(ERROR) << "Failed to find a frame with index " << current_frame
                    << " in Flac encoder. Data may be lost or corrupted.";
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
  }

  // Append to `audio_frame_` and track how many samples it represents. It will
  // be finalized later to ensure frames are finalized in chronological order.
  FlacFrame& flac_frame = flac_frame_iter->second;
  flac_frame.audio_frame_with_data->obu.audio_frame_.insert(
      flac_frame.audio_frame_with_data->obu.audio_frame_.end(), buffer,
      buffer + bytes);
  flac_frame.num_samples += samples;

  if (flac_frame.num_samples == flac_encoder->num_samples_per_frame_) {
    // A frame has been completed; move to the finalized frames.
    flac_encoder->finalized_audio_frames_.emplace_back(
        std::move(*flac_frame_iter->second.audio_frame_with_data));

    // The frame is fully processed and no longer needed.
    flac_encoder->frame_index_to_frame_.erase(flac_frame_iter);
  }

  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

void LibFlacMetadataCallback(const FLAC__StreamEncoder* /*encoder*/,
                             const FLAC__StreamMetadata* metadata,
                             void* client_data) {
  ABSL_LOG_FIRST_N(INFO, 1) << "Begin `LibFlacMetadataCallback`.";

  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    ABSL_LOG_FIRST_N(INFO, 1) << "Received `STREAMINFO` metadata.";
    // Just validate we got the `STREAMINFO` metadata at some point. IAMF
    // requires some fields to be set constant and different from what will be
    // returned by `libflac`.
    auto flac_encoder = static_cast<FlacEncoder*>(client_data);

    absl::MutexLock lock(flac_encoder->mutex_);
    flac_encoder->finished_ = true;
  }
}

FlacEncoder::~FlacEncoder() {
  FLAC__stream_encoder_delete(encoder_);

  absl::MutexLock lock(mutex_);
  if (!frame_index_to_frame_.empty()) {
    ABSL_LOG(ERROR)
        << "Some frames were not fully processed. Maybe `Finalize()` "
           "was not called.";
  }
}

absl::Status FlacEncoder::EncodeAudioFrame(
    const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateNotFinalized());
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  ABSL_LOG_FIRST_N(INFO, 1)
      << "num_samples_per_channel: " << num_samples_per_channel;
  ABSL_LOG_FIRST_N(INFO, 1)
      << "num_channels: " << channel_count_.num_channels();

  // FLAC requires a right-justified sign extended value. Calculate what the
  // mask is to sign extend a `input_bit_depth`-bit value.
  uint32_t base_sign_extension_mask = 0;
  for (int i = 31; i > input_pcm_bit_depth_ - 1; --i) {
    base_sign_extension_mask |= 1 << i;
  }

  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kLeftJustifiedToRightJustified = [&](int32_t input, int32_t& output) {
        // Only apply the sign extension mask when the left-justified value
        // has '1' in the MSB.
        const uint32_t sign_extension_mask =
            (input & 0x80000000) ? base_sign_extension_mask : 0;
        // Shift the input value to be right-justified.
        output = static_cast<uint32_t>(input) >> (32 - input_pcm_bit_depth_) |
                 sign_extension_mask;
        return absl::OkStatus();
      };

  // TODO(b/382197581): Avoid re-allocations of `encoder_input_pcm` and
  //                    `samples_spans`.
  // Convert input to the array that will be passed to `flac_encode`.
  std::vector<FLAC__int32> encoder_input_pcm;
  std::vector<absl::Span<const int32_t>> samples_spans(samples.size());
  for (int c = 0; c < samples.size(); c++) {
    samples_spans[c] = absl::MakeConstSpan(samples[c]);
  }
  RETURN_IF_NOT_OK(ConvertChannelTimeToInterleaved(
      absl::MakeConstSpan(samples_spans), encoder_input_pcm,
      kLeftJustifiedToRightJustified));

  ABSL_LOG_FIRST_N(INFO, 1)
      << "Encoding " << encoder_input_pcm.size() * 4 << " bytes representing "
      << num_samples_per_channel << " x " << channel_count_.num_channels()
      << " samples.";

  if (!FLAC__stream_encoder_process_interleaved(
          encoder_, encoder_input_pcm.data(), num_samples_per_channel)) {
    return absl::UnknownError("Flac failed to encode.");
  }

  absl::MutexLock lock(mutex_);

  // Transfer ownership of the partial audio frame so it can be finalized later.
  frame_index_to_frame_[next_frame_index_++].audio_frame_with_data =
      std::move(partial_audio_frame_with_data);
  return absl::OkStatus();
}

absl::Status FlacEncoder::Finalize() {
  // Signal to `libflac` the encoder is finished.
  if (!FLAC__stream_encoder_finish(encoder_)) {
    return absl::UnknownError("Failed to finalize Flac encoder.");
  }

  return absl::OkStatus();
}

absl::Status FlacEncoder::InitializeEncoder() {
  // Initialize the encoder.
  encoder_ = FLAC__stream_encoder_new();
  if (encoder_ == nullptr) {
    return absl::UnknownError("Failed to initialize Flac encoder.");
  }

  // Configure the FLAC encoder based on user input data.
  RETURN_IF_NOT_OK(Configure(encoder_metadata_, decoder_config_, channel_count_,
                             num_samples_per_frame_, output_sample_rate_,
                             input_pcm_bit_depth_, encoder_));

  // Initialize the FLAC encoder.
  FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
      encoder_, LibFlacWriteCallback, /*seek_callback=*/nullptr,
      /*tell_callback=*/nullptr, LibFlacMetadataCallback,
      static_cast<void*>(this));

  return FlacStreamEncoderInitStatusToAbslStatus(init_status);
}

}  // namespace iamf_tools
