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
#include "iamf/cli/codec/aac_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "iamf/obu/substream_channel_count.h"

// This symbol conflicts with `aacenc_lib.h` and `aacdecoder_lib.h`.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/aac_utils.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/types.h"
#include "libAACdec/include/aacdecoder_lib.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

using ::absl::MakeConstSpan;

// Converts an AAC_DECODER_ERROR to an absl::Status.
absl::Status AacDecoderErrorToAbslStatus(AAC_DECODER_ERROR aac_error_code,
                                         absl::string_view error_message) {
  absl::StatusCode status_code;
  switch (aac_error_code) {
    case AAC_DEC_OK:
      return absl::OkStatus();
    case AAC_DEC_OUT_OF_MEMORY:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    case AAC_DEC_TRANSPORT_SYNC_ERROR:
    case AAC_DEC_NOT_ENOUGH_BITS:
    case AAC_DEC_INVALID_HANDLE:
    case AAC_DEC_UNSUPPORTED_AOT:
    case AAC_DEC_UNSUPPORTED_FORMAT:
    case AAC_DEC_UNSUPPORTED_ER_FORMAT:
    case AAC_DEC_UNSUPPORTED_EPCONFIG:
    case AAC_DEC_UNSUPPORTED_MULTILAYER:
    case AAC_DEC_UNSUPPORTED_CHANNELCONFIG:
    case AAC_DEC_UNSUPPORTED_SAMPLINGRATE:
    case AAC_DEC_INVALID_SBR_CONFIG:
    case AAC_DEC_SET_PARAM_FAIL:
    case AAC_DEC_OUTPUT_BUFFER_TOO_SMALL:
    case AAC_DEC_UNSUPPORTED_EXTENSION_PAYLOAD:
    case AAC_DEC_UNSUPPORTED_SBA:
    case AAC_DEC_ANC_DATA_ERROR:
    case AAC_DEC_TOO_SMALL_ANC_BUFFER:
    case AAC_DEC_TOO_MANY_ANC_ELEMENTS:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AAC_DEC_NEED_TO_RESTART:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    // Several error codes usually imply that the bitstream is corrupt.
    case AAC_DEC_TRANSPORT_ERROR:
    case AAC_DEC_PARSE_ERROR:
    case AAC_DEC_DECODE_FRAME_ERROR:
    case AAC_DEC_INVALID_CODE_BOOK:
    case AAC_DEC_UNSUPPORTED_PREDICTION:
    case AAC_DEC_UNSUPPORTED_CCE:
    case AAC_DEC_UNSUPPORTED_LFE:
    case AAC_DEC_UNSUPPORTED_GAIN_CONTROL_DATA:
    case AAC_DEC_CRC_ERROR:
    case AAC_DEC_RVLC_ERROR:
    case AAC_DEC_TNS_READ_ERROR:
      status_code = absl::StatusCode::kDataLoss;
      break;
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }

  return absl::Status(
      status_code,
      absl::StrCat(error_message, " AAC_DECODER_ERROR= ", aac_error_code));
}

absl::Status ConfigureAacDecoder(const AacDecoderConfig& raw_aac_decoder_config,
                                 SubstreamChannelCount channel_count,
                                 AAC_DECODER_INSTANCE* decoder_) {
  // Configure `fdk_aac` with the audio specific config which has the correct
  // number of channels in it. IAMF may share a decoder config for several
  // substreams, so the raw value may not be accurate.
  AudioSpecificConfig fdk_audio_specific_config =
      raw_aac_decoder_config.decoder_specific_info_.audio_specific_config;
  fdk_audio_specific_config.channel_configuration_ =
      channel_count.num_channels();

  // Serialize the modified config. Assume a reasonable default size, but let
  // the buffer be resizable to be safe.
  const size_t kMaxAudioSpecificConfigSize = 5;
  WriteBitBuffer wb(kMaxAudioSpecificConfigSize);
  const absl::Status status = fdk_audio_specific_config.ValidateAndWrite(wb);

  if (status.ok() && wb.IsByteAligned()) {
    // Transform data from `const uint_t*` to `UCHAR*` to match the `libaac`
    // interface.
    std::vector<UCHAR> libaac_audio_specific_config(wb.bit_buffer().size());
    std::transform(wb.bit_buffer().begin(), wb.bit_buffer().end(),
                   libaac_audio_specific_config.begin(),
                   [](uint8_t c) { return static_cast<UCHAR>(c); });

    // Configure `decoder_` with the serialized data.
    UCHAR* conf[] = {libaac_audio_specific_config.data()};
    const UINT length[] = {static_cast<UINT>(wb.bit_offset() / 8)};
    aacDecoder_ConfigRaw(decoder_, conf, length);
  } else {
    ABSL_LOG(ERROR) << "Erroring writing audio specific config: " << status
                    << " wrote " << wb.bit_offset() << " bits.";
  }

  return status;
}

}  // namespace

absl::StatusOr<std::unique_ptr<DecoderBase>> AacDecoder::Create(
    const AacDecoderConfig& decoder_config, SubstreamChannelCount channel_count,
    uint32_t num_samples_per_frame) {
  // Initialize the decoder.
  AAC_DECODER_INSTANCE* decoder =
      aacDecoder_Open(GetAacTransportationType(), /*nrOfLayers=*/1);

  if (decoder == nullptr) {
    return absl::UnknownError("Failed to initialize AAC decoder.");
  }

  const auto status =
      ConfigureAacDecoder(decoder_config, channel_count, decoder);
  if (!status.ok()) {
    aacDecoder_Close(decoder);
    return status;
  }

  // Constrain the maximum number of output channels to match the IAMF-level
  // substream channel count.  Without this, fdk_aac's implicit SBR/PS
  // processing could increase the output channel count beyond what the
  // output buffer is sized for.
  aacDecoder_SetParam(decoder, AAC_PCM_MAX_OUTPUT_CHANNELS,
                      channel_count.num_channels());

  const auto* stream_info = aacDecoder_GetStreamInfo(decoder);
  ABSL_LOG_FIRST_N(INFO, 1) << "Created an AAC decoder with "
                            << stream_info->numChannels << " channels.";

  return absl::WrapUnique(
      new AacDecoder(channel_count, num_samples_per_frame, decoder));
}

AacDecoder::~AacDecoder() {
  // The factory function prevents `decoder_` from ever being null.
  ABSL_CHECK_NE(decoder_, nullptr);
  aacDecoder_Close(decoder_);
}

absl::Status AacDecoder::DecodeAudioFrame(
    absl::Span<const uint8_t> encoded_frame) {
  // Transform the data and feed it to the decoder.
  raws_frame_to_libfdk_aac_.resize(encoded_frame.size());
  std::transform(encoded_frame.begin(), encoded_frame.end(),
                 raws_frame_to_libfdk_aac_.begin(),
                 [](uint8_t c) { return static_cast<UCHAR>(c); });

  UCHAR* in_buffer[] = {raws_frame_to_libfdk_aac_.data()};
  const UINT buffer_size[] = {static_cast<UINT>(encoded_frame.size())};
  UINT bytes_valid = static_cast<UINT>(encoded_frame.size());
  RETURN_IF_NOT_OK(AacDecoderErrorToAbslStatus(
      aacDecoder_Fill(decoder_, in_buffer, buffer_size, &bytes_valid),
      "Failed on `aacDecoder_Fill`: "));
  if (bytes_valid != 0) {
    return absl::InvalidArgumentError(
        "The input frame failed to decode. It may not have been a "
        "complete AAC frame.");
  }

  // Retrieve the decoded frame. `fdk_aac` decodes to INT_PCM (usually 16-bits)
  // samples with channels interlaced.
  RETURN_IF_NOT_OK(AacDecoderErrorToAbslStatus(
      aacDecoder_DecodeFrame(decoder_, interleaved_pcm_from_libfdk_aac_.data(),
                             interleaved_pcm_from_libfdk_aac_.size(),
                             /*flags=*/0),
      "Failed on `aacDecoder_DecodeFrame`: "));

  // Arrange the interleaved data in (channel, time) axes with samples stored in
  // the upper bytes of an `int32_t`.
  const auto fdk_aac_bit_depth = GetFdkAacBitDepth();
  const absl::AnyInvocable<absl::Status(INT_PCM, InternalSampleType&) const>
      kAacInternalTypeToSampleType =
          [](INT_PCM input, InternalSampleType& output) {
            output = Int32ToNormalizedFloatingPoint<InternalSampleType>(
                static_cast<int32_t>(input) << (32 - fdk_aac_bit_depth));
            return absl::OkStatus();
          };
  return ConvertInterleavedToChannelTime(
      MakeConstSpan(interleaved_pcm_from_libfdk_aac_),
      channel_count_.num_channels(), decoded_samples_,
      kAacInternalTypeToSampleType);
}

}  // namespace iamf_tools
