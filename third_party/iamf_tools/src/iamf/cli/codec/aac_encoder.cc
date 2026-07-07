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
#include "iamf/cli/codec/aac_encoder.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/aac_utils.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/substream_channel_count.h"
#include "libAACenc/include/aacenc_lib.h"
#include "libSYS/include/FDK_audio.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

// Converts an AACENC_ERROR to an absl::Status.
absl::Status AacEncErrorToAbslStatus(AACENC_ERROR aac_error_code,
                                     const absl::string_view error_message) {
  absl::StatusCode status_code;
  switch (aac_error_code) {
    case AACENC_OK:
      return absl::OkStatus();
    case AACENC_INVALID_HANDLE:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_MEMORY_ERROR:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    case AACENC_UNSUPPORTED_PARAMETER:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_INVALID_CONFIG:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    case AACENC_INIT_ERROR:
    case AACENC_INIT_AAC_ERROR:
    case AACENC_INIT_SBR_ERROR:
    case AACENC_INIT_TP_ERROR:
    case AACENC_INIT_META_ERROR:
    case AACENC_INIT_MPS_ERROR:
      status_code = absl::StatusCode::kInternal;
      break;
    case AACENC_ENCODE_EOF:
      status_code = absl::StatusCode::kOutOfRange;
      break;
    case AACENC_ENCODE_ERROR:
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }

  return absl::Status(
      status_code,
      absl::StrCat(error_message, " AACENC_ERROR= ", aac_error_code));
}

absl::Status ConfigureAacEncoder(
    const iamf_tools_cli_proto::AacEncoderMetadata& encoder_metadata,
    SubstreamChannelCount channel_count, uint32_t output_sample_rate,
    AACENCODER* const encoder) {
  // IAMF requires metadata is not embedded in the stream.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_METADATA_MODE, 0),
      "Failed to configure encoder metadata mode."));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_TRANSMUX, GetAacTransportationType()),
      "Failed to configure encoder transport type."));

  // IAMF only supports AAC-LC.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_AOT, AOT_AAC_LC),
      "Failed to configure encoder audio object type."));

  // Configure values based on the associated Codec Config OBU.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_SAMPLERATE,
                          static_cast<UINT>(output_sample_rate)),
      "Failed to configure encoder sample rate."));

  CHANNEL_MODE aac_channel_mode;
  switch (channel_count.num_channels()) {
    case 1:
      aac_channel_mode = MODE_1;

      break;
    case 2:
      aac_channel_mode = MODE_2;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("IAMF requires AAC to be used with 1 or 2 channels. Got "
                       "num_channels= ",
                       channel_count.num_channels()));
  }
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, aac_channel_mode),
      absl::StrCat("Failed to configure encoder channel mode= ",
                   aac_channel_mode)));

  // Let AACENC_BITRATE be configured automatically.

  // Set some arguments configured by the user-provided `encoder_metadata_`.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_AFTERBURNER,
                          encoder_metadata.enable_afterburner() ? 1 : 0),
      absl::StrCat(
          "Failed to configure encoder afterburner enable_afterburner= ",
          encoder_metadata.enable_afterburner())));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_BITRATEMODE,
                          encoder_metadata.bitrate_mode()),
      absl::StrCat("Failed to configure encoder bitrate mode= ",
                   encoder_metadata.bitrate_mode())));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_SIGNALING_MODE,
                          encoder_metadata.signaling_mode()),
      absl::StrCat("Failed to configure encoder signaling mode= ",
                   encoder_metadata.signaling_mode())));

  return absl::OkStatus();
}

absl::Status ValidateEncoderInfo(int num_channels,
                                 uint32_t num_samples_per_frame,
                                 AACENCODER* const encoder) {
  // Validate the configuration is consistent with the associated Codec Config
  // OBU.
  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder, &enc_info),
                                           "Failed to get encoder info."));

  RETURN_IF_NOT_OK(ValidateEqual<size_t>(
      num_channels, enc_info.inputChannels,
      "user requested vs libFDK required `num_channels`"));
  RETURN_IF_NOT_OK(ValidateEqual(
      num_samples_per_frame, static_cast<uint32_t>(enc_info.frameLength),
      "user requested vs libFDK required `num_samples_per_frame`"));

  return absl::OkStatus();
}

}  // namespace

absl::Status AacEncoder::InitializeEncoder() {
  if (encoder_) {
    return absl::InvalidArgumentError(
        "Expected `encoder_` to not be initialized yet.");
  }

  // Open the encoder.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncOpen(&encoder_, 0, channel_count_.num_channels()),
      "Failed to initialize AAC encoder."));

  // Configure the encoder.
  RETURN_IF_NOT_OK(ConfigureAacEncoder(encoder_metadata_, channel_count_,
                                       output_sample_rate_, encoder_));

  // Call `aacEncEncode` with `nullptr` arguments to initialize the encoder.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncEncode(encoder_, nullptr, nullptr, nullptr, nullptr),
      "Failed on call to `aacEncEncode`."));

  // Validate the configuration matches expected results.
  RETURN_IF_NOT_OK(ValidateEncoderInfo(channel_count_.num_channels(),
                                       num_samples_per_frame_, encoder_));

  return absl::OkStatus();
}

AacEncoder::~AacEncoder() { aacEncClose(&encoder_); }

absl::Status AacEncoder::EncodeAudioFrame(
    const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  if (!encoder_) {
    ABSL_LOG(ERROR) << "Expected `encoder_` to be initialized.";
  }
  RETURN_IF_NOT_OK(ValidateNotFinalized());
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder_, &enc_info),
                                           "Failed to get encoder info."));

  // Convert input to the array that will be passed to `aacEncEncode`.
  if (input_pcm_bit_depth_ != GetFdkAacBitDepth()) {
    auto error_message =
        absl::StrCat("Expected AAC to be ", GetFdkAacBitDepth(), " bits, got ",
                     input_pcm_bit_depth_);
    return absl::InvalidArgumentError(error_message);
  }

  // `fdk_aac` requires the native system endianness as input.
  const bool big_endian = IsNativeBigEndian();

  // TODO(b/382197581): Avoid re-allocations of `encoder_input_pcm`.
  std::vector<INT_PCM> encoder_input_pcm(
      num_samples_per_channel * channel_count_.num_channels(), 0);
  size_t write_position = 0;
  for (int t = 0; t < samples[0].size(); ++t) {
    for (int c = 0; c < samples.size(); c++) {
      // Convert all frames to INT_PCM samples for input for `fdk_aac` (usually
      // 16-bit).
      RETURN_IF_NOT_OK(WritePcmSample(
          static_cast<uint32_t>(samples[c][t]), input_pcm_bit_depth_,
          big_endian, reinterpret_cast<uint8_t*>(encoder_input_pcm.data()),
          write_position));
    }
  }

  // The `fdk_aac` interface supports multiple input buffers. Although IAMF only
  // uses one buffer without metadata or ancillary data.
  void* in_buffers[1] = {encoder_input_pcm.data()};
  INT in_buffer_identifiers[1] = {IN_AUDIO_DATA};
  INT in_buffer_sizes[1] = {
      static_cast<INT>(encoder_input_pcm.size() * GetFdkAacBytesPerSample())};
  INT in_buffer_element_sizes[1] = {GetFdkAacBytesPerSample()};
  AACENC_BufDesc in_buffer_desc = {.numBufs = 1,
                                   .bufs = in_buffers,
                                   .bufferIdentifiers = in_buffer_identifiers,
                                   .bufSizes = in_buffer_sizes,
                                   .bufElSizes = in_buffer_element_sizes};
  AACENC_InArgs in_args = {
      .numInSamples = static_cast<INT>(num_samples_per_channel *
                                       channel_count_.num_channels()),
      .numAncBytes = 0};

  // Resize the output buffer to support the worst case size.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(enc_info.maxOutBufBytes, 0);

  // The `fdk_aac` interface supports multiple input buffers. Although IAMF only
  // uses one buffer without metadata or ancillary data.
  void* out_bufs[1] = {audio_frame.data()};
  INT out_buffer_identifiers[1] = {OUT_BITSTREAM_DATA};
  INT out_buffer_sizes[1] = {
      static_cast<INT>(audio_frame.size() * sizeof(uint8_t))};
  INT out_buffer_element_sizes[1] = {sizeof(uint8_t)};
  AACENC_BufDesc out_buffer_desc = {.numBufs = 1,
                                    .bufs = out_bufs,
                                    .bufferIdentifiers = out_buffer_identifiers,
                                    .bufSizes = out_buffer_sizes,
                                    .bufElSizes = out_buffer_element_sizes};

  // Encode the frame.
  AACENC_OutArgs out_args;
  // This implementation expects `fdk_aac` to return an entire frame and no
  // error code.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncEncode(encoder_, &in_buffer_desc, &out_buffer_desc, &in_args,
                   &out_args),
      "Failed on call to `aacEncEncode`."));

  if (num_samples_per_channel * channel_count_.num_channels() !=
      out_args.numInSamples) {
    return absl::UnknownError("Failed to encode an entire frame.");
  }

  // Resize the buffer to the actual size and finalize it.
  audio_frame.resize(out_args.numOutBytes);
  absl::MutexLock lock(mutex_);
  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  ABSL_LOG_FIRST_N(INFO, 1)
      << "Encoded " << num_samples_per_channel << " samples * "
      << channel_count_.num_channels() << " channels using "
      << out_args.numOutBytes << " bytes";
  return absl::OkStatus();
}

absl::Status AacEncoder::SetNumberOfSamplesToDelayAtStart(
    bool /*validate_codec_delay*/) {
  if (!encoder_) {
    ABSL_LOG(ERROR) << "Expected `encoder_` to be initialized.";
  }

  // Validate the configuration.
  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder_, &enc_info),
                                           "Failed to get encoder info."));

  // Set the number of samples the decoder must ignore. For AAC this appears
  // to be implementation specific. The implementation of AAC-LC in `fdk_aac`
  // seems to usually make this 2048 samples.
  required_samples_to_delay_at_start_ = enc_info.nDelayCore;
  return absl::OkStatus();
}

}  // namespace iamf_tools
