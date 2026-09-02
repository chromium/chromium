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
#include "iamf/obu/decoder_config/flac_decoder_config.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using ::absl::MakeConstSpan;
using ::absl::MakeSpan;

absl::Status GetStreamInfo(const FlacDecoderConfig& decoder_config,
                           const FlacMetaBlockStreamInfo** stream_info) {
  if (decoder_config.metadata_blocks_.empty() ||
      decoder_config.metadata_blocks_.front().header.block_type !=
          FlacMetaBlockHeader::kFlacStreamInfo) {
    return absl::InvalidArgumentError(
        "FLAC always requires the first block is present and is a "
        "`STREAMINFO` block.");
  }

  *stream_info = &std::get<FlacMetaBlockStreamInfo>(
      decoder_config.metadata_blocks_.front().payload);
  return absl::OkStatus();
}

using StrictCons = FlacStreamInfoStrictConstraints;

absl::Status ValidateSampleRate(uint32_t sample_rate) {
  // SHALL be set to one of the common sample rates indicated by 0b0001 to
  // 0b1011 in 9.1.2. Sample Rate Bits of [RFC-9639].
  //
  // Set is arranged in the order as listed in the related RFC.
  static const absl::NoDestructor<absl::flat_hash_set<uint32_t>>
      kAllowedSampleRates({88200, 176400, 192000, 8000, 16000, 22050, 24000,
                           32000, 44100, 48000, 96000});
  if (kAllowedSampleRates->contains(sample_rate)) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid sample rate: ", sample_rate));
}

absl::Status ValidateBitsPerSample(uint8_t raw_bits_per_sample) {
  // SHALL be set to one of 15, 23 or 31, corresponding to 16, 24 or 32 bits
  // respectively.
  static const absl::NoDestructor<absl::flat_hash_set<uint8_t>>
      kAllowedBitsPerSample({15, 23, 31});
  if (kAllowedBitsPerSample->contains(raw_bits_per_sample)) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid bits per sample: ", raw_bits_per_sample));
}

absl::Status ValidateTotalSamplesInStream(uint64_t total_samples_in_stream) {
  // The FLAC specification treats this as a 36-bit value which is always valid,
  // but in `iamf_tools` it could be out of bounds because it is stored as a
  // `uint64_t`.
  return ValidateInRange(total_samples_in_stream,
                         {StrictCons::kMinTotalSamplesInStream,
                          StrictCons::kMaxTotalSamplesInStream},
                         "total_samples_in_stream");
}

// Validates the `FlacDecoderConfig` for decoding. To be robust and encode files
// that are not valid, some restrictions are relaxed.
absl::Status ValidateDecodingRestrictions(
    uint32_t num_samples_per_frame, const FlacDecoderConfig& decoder_config) {
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(decoder_config, &stream_info));

  // FLAC restricts some fields.
  RETURN_IF_NOT_OK(ValidateSampleRate(stream_info->sample_rate));
  RETURN_IF_NOT_OK(ValidateBitsPerSample(stream_info->bits_per_sample));

  // IAMF restricts some fields.
  RETURN_IF_NOT_OK(
      ValidateEqual(static_cast<uint32_t>(stream_info->maximum_block_size),
                    num_samples_per_frame, "maximum_block_size"));
  RETURN_IF_NOT_OK(
      ValidateEqual(static_cast<uint32_t>(stream_info->minimum_block_size),
                    num_samples_per_frame, "minimum_block_size"));

  RETURN_IF_NOT_OK(ValidateEqual(stream_info->number_of_channels,
                                 StrictCons::kNumberOfChannels,
                                 "number_of_channels"));

  return ValidateTotalSamplesInStream(stream_info->total_samples_in_stream);
}

// Validates the `FlacDecoderConfig` for encoding, typically we want to enforce
// both the strict and looser constraints. It's best not to encode or allow
// producing files that are strange.
absl::Status ValidateEncodingRestrictions(
    uint32_t num_samples_per_frame, const FlacDecoderConfig& decoder_config) {
  // Validate the stricter stricter constaints also used when decoding.
  RETURN_IF_NOT_OK(
      ValidateDecodingRestrictions(num_samples_per_frame, decoder_config));
  using LooseCons = FlacStreamInfoLooseConstraints;

  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(decoder_config, &stream_info));

  // The IAMF spec instruct there values "SHOULD" agree. During encoding we take
  // this strictly, to avoid producing files that are strange.
  RETURN_IF_NOT_OK(ValidateEqual(stream_info->minimum_frame_size,
                                 LooseCons::kMinFrameSize,
                                 "minimum_frame_size"));
  RETURN_IF_NOT_OK(ValidateEqual(stream_info->maximum_frame_size,
                                 LooseCons::kMaxFrameSize,
                                 "maximum_frame_size"));

  if (stream_info->md5_signature != LooseCons::kMd5Signature) {
    return absl::InvalidArgumentError("Invalid md5_signature.");
  }

  return absl::OkStatus();
}

absl::Status ValidateAudioRollDistance(int16_t audio_roll_distance) {
  return ValidateEqual(audio_roll_distance,
                       FlacDecoderConfig::GetRequiredAudioRollDistance(),
                       "audio_roll_distance");
}

absl::Status WriteStreamInfo(const FlacMetaBlockStreamInfo& stream_info,
                             WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.minimum_block_size, 16));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.maximum_block_size, 16));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.minimum_frame_size, 24));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.maximum_frame_size, 24));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.sample_rate, 20));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.number_of_channels, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.bits_per_sample, 5));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral64(stream_info.total_samples_in_stream, 36));
  RETURN_IF_NOT_OK(wb.WriteUint8Span(MakeConstSpan(stream_info.md5_signature)));
  return absl::OkStatus();
}

void PrintStreamInfo(const FlacMetaBlockStreamInfo& stream_info) {
  ABSL_VLOG(1) << "      metadata_block(stream_info):";

  ABSL_VLOG(1) << "        minimum_block_size= "
               << stream_info.minimum_block_size;
  ABSL_VLOG(1) << "        maximum_block_size= "
               << stream_info.maximum_block_size;
  ABSL_VLOG(1) << "        minimum_frame_size= "
               << stream_info.minimum_frame_size;
  ABSL_VLOG(1) << "        maximum_frame_size= "
               << stream_info.maximum_frame_size;
  ABSL_VLOG(1) << "        sample_rate= " << stream_info.sample_rate;
  ABSL_VLOG(1) << "        number_of_channels= "
               << absl::StrCat(stream_info.number_of_channels);
  ABSL_VLOG(1) << "        bits_per_sample= "
               << absl::StrCat(stream_info.bits_per_sample);
  ABSL_VLOG(1) << "        total_samples_in_stream= "
               << stream_info.total_samples_in_stream;
}

absl::Status ReadStreamInfo(FlacMetaBlockStreamInfo& stream_info,
                            ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(16, stream_info.minimum_block_size));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(16, stream_info.maximum_block_size));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, stream_info.minimum_frame_size));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, stream_info.maximum_frame_size));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(20, stream_info.sample_rate));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, stream_info.number_of_channels));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, stream_info.bits_per_sample));
  RETURN_IF_NOT_OK(
      rb.ReadUnsignedLiteral(36, stream_info.total_samples_in_stream));
  return rb.ReadUint8Span(absl::MakeSpan(stream_info.md5_signature));
}

}  // namespace

absl::Status FlacDecoderConfig::ValidateAndWrite(uint32_t num_samples_per_frame,
                                                 int16_t audio_roll_distance,
                                                 WriteBitBuffer& wb) const {
  MAYBE_RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));

  RETURN_IF_NOT_OK(ValidateEncodingRestrictions(num_samples_per_frame, *this));

  for (size_t i = 0; i < metadata_blocks_.size(); ++i) {
    const auto& metadata_block = metadata_blocks_[i];
    const bool is_last_block = (i == metadata_blocks_.size() - 1);
    RETURN_IF_NOT_OK(wb.WriteBoolean(is_last_block));
    RETURN_IF_NOT_OK(
        wb.WriteUnsignedLiteral(metadata_block.header.block_type, 7));

    switch (metadata_block.header.block_type) {
      case FlacMetaBlockHeader::kFlacStreamInfo: {
        auto* stream_info =
            std::get_if<FlacMetaBlockStreamInfo>(&metadata_block.payload);
        RETURN_IF_NOT_OK(ValidateNotNull(
            stream_info,
            "Stream info payload is not a `FlacMetaBlockStreamInfo`."));
        RETURN_IF_NOT_OK(
            wb.WriteUnsignedLiteral(StrictCons::kStreamInfoBlockLength, 24));
        RETURN_IF_NOT_OK(WriteStreamInfo(*stream_info, wb));
      } break;
      default: {
        auto* generic_block =
            std::get_if<std::vector<uint8_t>>(&metadata_block.payload);
        RETURN_IF_NOT_OK(ValidateNotNull(
            generic_block,
            "Generic block payload is not a `FlacMetaBlockStreamInfo`."));
        RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(generic_block->size(), 24));
        RETURN_IF_NOT_OK(wb.WriteUint8Span(MakeConstSpan(*generic_block)));
        break;
      }
    }
  }

  return absl::OkStatus();
}

absl::Status FlacDecoderConfig::ReadAndValidate(uint32_t num_samples_per_frame,
                                                int16_t audio_roll_distance,
                                                ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));

  // We are not given a length field to indicate the number of metadata blocks
  // to read. Instead, we must look at the `last_metadata_block_flag` to
  // determine when to stop reading.  Cap at 128 blocks as a safety measure
  // against crafted bitstreams — the FLAC spec defines only 7 block types.
  static constexpr int kMaxMetadataBlocks = 128;
  std::vector<FlacMetadataBlock> metadata_blocks;
  bool last_metadata_block_flag = false;
  int metadata_block_count = 0;
  while (!last_metadata_block_flag) {
    if (++metadata_block_count > kMaxMetadataBlocks) {
      return absl::InvalidArgumentError(
          "Too many FLAC metadata blocks without last_metadata_block_flag.");
    }
    FlacMetadataBlock metadata_block;
    RETURN_IF_NOT_OK(rb.ReadBoolean(last_metadata_block_flag));
    uint8_t block_type;
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, block_type));
    metadata_block.header.block_type =
        static_cast<FlacMetaBlockHeader::FlacBlockType>(block_type);
    uint32_t metadata_data_block_length;
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, metadata_data_block_length));

    switch (metadata_block.header.block_type) {
      case FlacMetaBlockHeader::kFlacStreamInfo:
        RETURN_IF_NOT_OK(ReadStreamInfo(
            std::get<FlacMetaBlockStreamInfo>(metadata_block.payload), rb));
        break;
      default: {
        // Bound the allocation to the maximum OBU size before reserving, as
        // the 24-bit length is otherwise attacker-controlled up to 16 MB with
        // no relation to the bytes actually available. Mirrors the guards in
        // the sibling decoder-config readers.
        if (metadata_data_block_length > kEntireObuSizeMaxTwoMegabytes) {
          return absl::InvalidArgumentError(absl::StrCat(
              "metadata_data_block_length= ", metadata_data_block_length,
              " exceeds maximum."));
        }
        std::vector<uint8_t> payload(metadata_data_block_length);
        RETURN_IF_NOT_OK(rb.ReadUint8Span(MakeSpan(payload)));
        metadata_block.payload = std::move(payload);
        break;
      }
    }
    metadata_blocks_.push_back(std::move(metadata_block));
  }
  RETURN_IF_NOT_OK(ValidateDecodingRestrictions(num_samples_per_frame, *this));
  return absl::OkStatus();
}

absl::Status FlacDecoderConfig::GetOutputSampleRate(
    uint32_t& output_sample_rate) const {
  output_sample_rate = 0;
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));
  RETURN_IF_NOT_OK(ValidateSampleRate(stream_info->sample_rate));

  output_sample_rate = stream_info->sample_rate;
  return absl::OkStatus();
}

absl::Status FlacDecoderConfig::GetBitDepthToMeasureLoudness(
    uint8_t& bit_depth_to_measure_loudness) const {
  bit_depth_to_measure_loudness = 0;
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));
  RETURN_IF_NOT_OK(ValidateBitsPerSample(stream_info->bits_per_sample));

  // The raw bit-depth field for FLAC represents bit-depth - 1.
  bit_depth_to_measure_loudness = stream_info->bits_per_sample + 1;
  return absl::OkStatus();
}

absl::Status FlacDecoderConfig::GetTotalSamplesInStream(
    uint64_t& total_samples_in_stream) const {
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));

  total_samples_in_stream = stream_info->total_samples_in_stream;
  return ValidateTotalSamplesInStream(stream_info->total_samples_in_stream);
}

void FlacDecoderConfig::Print() const {
  ABSL_VLOG(1) << "    decoder_config(flac):";

  for (const auto& metadata_block : metadata_blocks_) {
    ABSL_VLOG(1) << "      header:";
    ABSL_VLOG(1) << "        last_metadata_block_flag (omitted).";
    ABSL_VLOG(1) << "        block_type= "
                 << absl::StrCat(metadata_block.header.block_type);

    switch (metadata_block.header.block_type) {
      case FlacMetaBlockHeader::kFlacStreamInfo:
        ABSL_VLOG(1) << "        metadata_data_block_length= "
                     << StrictCons::kStreamInfoBlockLength;
        PrintStreamInfo(
            std::get<FlacMetaBlockStreamInfo>(metadata_block.payload));
        break;
      default: {
        const auto& generic_block =
            std::get<std::vector<uint8_t>>(metadata_block.payload);
        ABSL_VLOG(1) << "        metadata_data_block_length= "
                     << generic_block.size();
        ABSL_VLOG(1) << "      metadata_block(generic_block):";
        ABSL_VLOG(1) << "        size= " << generic_block.size();
        ABSL_VLOG(1) << "        payload omitted.";
      }
    }
  }
}

}  // namespace iamf_tools
