/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/wav_sample_provider.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

void FillChannelIds(
    const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata,
    std::vector<uint32_t>& channel_ids) {
  // Collect the channel IDs.
  channel_ids.reserve(audio_frame_metadata.channel_metadatas().size());
  for (const auto& channel_metadata :
       audio_frame_metadata.channel_metadatas()) {
    channel_ids.push_back(channel_metadata.channel_id());
  }

  if (!ValidateUnique(channel_ids.begin(), channel_ids.end(), "channel ids")
           .ok()) {
    // OK. The user is claiming some channel IDs are shared between labels.
    // This is strange, but permitted.
    ABSL_LOG(WARNING) << "Usually channel labels should be unique. Did you use "
                         "the same channel ID for different channels?";
  };
}

absl::Status ValidateWavReaderIsConsistentWithData(
    absl::string_view wav_filename_for_debugging, const WavReader& wav_reader,
    const CodecConfigObu& codec_config,
    const std::vector<uint32_t>& channel_ids) {
  const std::string pretty_print_wav_filename =
      absl::StrCat("WAV (", wav_filename_for_debugging, ")");
  const int encoder_input_pcm_bit_depth =
      static_cast<int>(codec_config.GetBitDepthToMeasureLoudness());
  if (wav_reader.bit_depth() > encoder_input_pcm_bit_depth) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Refusing to lower bit-depth of ", pretty_print_wav_filename,
        " with bit_depth= ", wav_reader.bit_depth(),
        " to bit_depth=", encoder_input_pcm_bit_depth));
  }

  const uint32_t encoder_input_sample_rate = codec_config.GetInputSampleRate();
  if (wav_reader.sample_rate_hz() != encoder_input_sample_rate) {
    return absl::InvalidArgumentError(absl::StrCat(
        pretty_print_wav_filename, "has a sample rate of ",
        wav_reader.sample_rate_hz(), " Hz. Expected a sample rate of ",
        encoder_input_sample_rate,
        " Hz based on the Codec Config OBU. Consider using a third party "
        "resampler on the WAV file, or picking Codec Config OBU settings to "
        "match the WAV file before trying again."));
  }

  const uint32_t decoder_output_sample_rate =
      codec_config.GetOutputSampleRate();
  if (encoder_input_sample_rate != decoder_output_sample_rate) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Input and output sample rates differ: (", encoder_input_sample_rate,
        " vs ", decoder_output_sample_rate, ")"));
  }

  // To prevent indexing out of bounds after the `WavSampleProvider` is
  // created, we ensure all user-specified channel IDs are in range of the
  // number of channels in the input file.
  for (const uint32_t channel_id : channel_ids) {
    if (channel_id >= wav_reader.num_channels()) {
      return absl::InvalidArgumentError(
          absl::StrCat(pretty_print_wav_filename,
                       " has num_channels= ", wav_reader.num_channels(),
                       ". channel_id= ", channel_id, " is out of bounds."));
    }
  }

  return absl::OkStatus();
}

// Fills in `channel_ids`, `labels`, and creates a `WavReader` from the input
// metadata and other input data.
absl::Status InitializeForAudioElement(
    uint32_t audio_element_id,
    const iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata,
    const std::string& wav_filename, const CodecConfigObu& codec_config,
    std::vector<uint32_t>& channel_ids,
    std::vector<ChannelLabel::Label>& labels,
    absl::flat_hash_map<DecodedUleb128, WavReader>&
        audio_element_id_to_wav_reader) {
  FillChannelIds(audio_frame_metadata, channel_ids);
  RETURN_IF_NOT_OK(ChannelLabelUtils::ConvertAndFillLabels(
      audio_frame_metadata.channel_metadatas(), labels));

  auto wav_reader = WavReader::CreateFromFile(
      wav_filename, static_cast<size_t>(codec_config.GetNumSamplesPerFrame()));
  if (!wav_reader.ok()) {
    return wav_reader.status();
  }
  RETURN_IF_NOT_OK(ValidateWavReaderIsConsistentWithData(
      wav_filename, *wav_reader, codec_config, channel_ids));

  audio_element_id_to_wav_reader.emplace(audio_element_id,
                                         std::move(*wav_reader));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<WavSampleProvider> WavSampleProvider::Create(
    const ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadata,
    absl::string_view input_wav_directory,
    const DescriptorObus::AudioElementsById& audio_elements) {
  // Precompute, validate, and cache data for each audio element.
  absl::flat_hash_map<DecodedUleb128, WavReader> wav_readers;
  absl::flat_hash_map<DecodedUleb128, std::vector<uint32_t>>
      audio_element_id_to_channel_ids;
  absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>
      audio_element_id_to_labels;

  const std::filesystem::path input_wav_directory_path(input_wav_directory);
  for (const auto& audio_frame_obu_metadata : audio_frame_metadata) {
    const uint32_t audio_element_id =
        audio_frame_obu_metadata.audio_element_id();
    const auto& wav_filename =
        input_wav_directory_path /
        std::filesystem::path(audio_frame_obu_metadata.wav_filename());

    // Retrieve the Codec Config OBU for the audio element.
    auto audio_element_iter = audio_elements.find(audio_element_id);
    if (audio_element_iter == audio_elements.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("No Audio Element found for ID= ",
                       audio_frame_obu_metadata.audio_element_id()));
    }
    const CodecConfigObu* codec_config =
        audio_element_iter->second.codec_config;
    if (codec_config == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrCat("No Codec Config found for Audio Element ID= ",
                       audio_frame_obu_metadata.audio_element_id()));
    }

    auto [channel_ids_iter, inserted] = audio_element_id_to_channel_ids.emplace(
        audio_element_id, std::vector<uint32_t>());
    if (!inserted) {
      return absl::InvalidArgumentError(
          absl::StrCat("List of AudioFrameObuMetadatahas contains duplicate "
                       "Audio Element ID= ",
                       audio_element_id));
    }
    // Internals add to the maps in parallel; if one had an empty slot, then
    // the others will have an empty slot.

    RETURN_IF_NOT_OK(InitializeForAudioElement(
        audio_element_id, audio_frame_obu_metadata, wav_filename.string(),
        *codec_config, channel_ids_iter->second,
        audio_element_id_to_labels[audio_element_id], wav_readers));
  }
  return WavSampleProvider(std::move(wav_readers),
                           std::move(audio_element_id_to_channel_ids),
                           std::move(audio_element_id_to_labels));
}

absl::Status WavSampleProvider::ReadFrames(
    const DecodedUleb128 audio_element_id, LabelSamplesMap& labeled_samples,
    bool& finished_reading) {
  auto wav_reader_iter = wav_readers_.find(audio_element_id);
  if (wav_reader_iter == wav_readers_.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "No WAV reader found for Audio Element ID= ", audio_element_id));
  }
  auto& wav_reader = wav_reader_iter->second;
  const size_t samples_read = wav_reader.ReadFrame();
  ABSL_LOG_FIRST_N(INFO, 1) << samples_read << " samples read";

  // Note if the WAV reader is found for the Audio Element ID, then it's
  // guaranteed to have the other corresponding metadata (otherwise the
  // `Create()` would have failed).
  const size_t num_time_ticks = samples_read / wav_reader.num_channels();
  const auto& channel_ids =
      audio_element_id_to_channel_ids_.at(audio_element_id);
  const auto& channel_labels = audio_element_id_to_labels_.at(audio_element_id);
  labeled_samples.clear();
  for (int c = 0; c < channel_labels.size(); ++c) {
    auto& samples = labeled_samples[channel_labels[c]];
    samples.resize(num_time_ticks);
    const auto& wav_reader_buffer_for_channel =
        wav_reader.buffers_[channel_ids[c]];
    for (int t = 0; t < num_time_ticks; ++t) {
      samples[t] = Int32ToNormalizedFloatingPoint<InternalSampleType>(
          wav_reader_buffer_for_channel[t]);
    }
  }
  finished_reading = (wav_reader.remaining_samples() == 0);

  return absl::OkStatus();
}

WavSampleProvider::WavSampleProvider(
    absl::flat_hash_map<DecodedUleb128, WavReader>&& wav_readers,
    absl::flat_hash_map<DecodedUleb128, std::vector<uint32_t>>&&
        audio_element_id_to_channel_ids,
    absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>&&
        audio_element_id_to_labels)
    : wav_readers_(std::move(wav_readers)),
      audio_element_id_to_channel_ids_(
          std::move(audio_element_id_to_channel_ids)),
      audio_element_id_to_labels_(std::move(audio_element_id_to_labels)) {};

}  // namespace iamf_tools
