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
#include "iamf/cli/demixing_manager.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <list>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixer.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using enum ChannelLabel::Label;
using DemixingMetadataForAudioElementId =
    DemixingManager::DemixingMetadataForAudioElementId;

absl::Status FillRequiredDemixingMetadata(
    const absl::flat_hash_set<ChannelLabel::Label>& labels_to_demix,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const LabelGainMap& label_to_output_gain,
    DemixingMetadataForAudioElementId& demixing_metadata) {
  auto& demixers = demixing_metadata.demixers;

  if (!demixers.empty()) {
    return absl::UnknownError(
        "`FillRequiredDemixingMetadata()` should only be called once per Audio "
        "Element ID");
  }
  demixing_metadata.substream_id_to_labels = substream_id_to_labels;
  demixing_metadata.label_to_output_gain = label_to_output_gain;

  // Find the input surround number.
  int input_surround_number = 0;
  if (labels_to_demix.contains(kL7)) {
    input_surround_number = 7;
  } else if (labels_to_demix.contains(kL5)) {
    input_surround_number = 5;
  } else if (labels_to_demix.contains(kL3)) {
    input_surround_number = 3;
  } else if (labels_to_demix.contains(kL2)) {
    input_surround_number = 2;
  } else if (labels_to_demix.contains(kMono)) {
    input_surround_number = 1;
  }

  // Find the lowest output surround number.
  int output_lowest_surround_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       demixing_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kL7) != labels.end() &&
        output_lowest_surround_number > 7) {
      output_lowest_surround_number = 7;
    } else if (std::find(labels.begin(), labels.end(), kL5) != labels.end() &&
               output_lowest_surround_number > 5) {
      output_lowest_surround_number = 5;
    } else if (std::find(labels.begin(), labels.end(), kL3) != labels.end() &&
               output_lowest_surround_number > 3) {
      output_lowest_surround_number = 3;
    } else if (std::find(labels.begin(), labels.end(), kL2) != labels.end() &&
               output_lowest_surround_number > 2) {
      output_lowest_surround_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kMono) != labels.end() &&
               output_lowest_surround_number > 1) {
      output_lowest_surround_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }
  ABSL_VLOG(1) << "Surround demixers from S" << input_surround_number << " to S"
               << output_lowest_surround_number << " needed:";
  for (int surround_number = input_surround_number;
       surround_number > output_lowest_surround_number; surround_number--) {
    if (surround_number == 7) {
      demixers.push_front(S5ToS7Demixer);
      ABSL_VLOG(1) << "  S5ToS7Demixer added";
    } else if (surround_number == 5) {
      demixers.push_front(S3ToS5Demixer);
      ABSL_VLOG(1) << "  S3ToS5Demixer added";
    } else if (surround_number == 3) {
      demixers.push_front(S2ToS3Demixer);
      ABSL_VLOG(1) << "  S2ToS3Demixer added";
    } else if (surround_number == 2) {
      demixers.push_front(S1ToS2Demixer);
      ABSL_VLOG(1) << "  S1ToS2Demixer added";
    }
  }

  // Find the input height number. Artificially defining the height number of
  // "TF2" as 1.
  int input_height_number = 0;
  if (labels_to_demix.contains(kLtf4)) {
    input_height_number = 4;
  } else if (labels_to_demix.contains(kLtf2)) {
    input_height_number = 2;
  } else if (labels_to_demix.contains(kLtf3)) {
    input_height_number = 1;
  }

  // Find the lowest output height number.
  int output_lowest_height_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       demixing_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kLtf4) != labels.end() &&
        output_lowest_height_number > 4) {
      output_lowest_height_number = 4;
    } else if (std::find(labels.begin(), labels.end(), kLtf2) != labels.end() &&
               output_lowest_height_number > 2) {
      output_lowest_height_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kLtf3) != labels.end() &&
               output_lowest_height_number > 1) {
      output_lowest_height_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }

  // Collect demixers in a separate list first and append the list to the
  // output later. Height demixers need to be in reverse order as height
  // down-mixers but should go after the surround demixers.
  ABSL_VLOG(1) << "Height demixers from T" << input_height_number << " to "
               << (output_lowest_height_number == 2 ? "T2" : "TF3")
               << " needed:";
  std::list<Demixer> height_demixers;
  for (int height_number = input_height_number;
       height_number > output_lowest_height_number; height_number--) {
    if (height_number == 4) {
      height_demixers.push_front(T2ToT4Demixer);
      ABSL_VLOG(1) << "  T2ToT4Demixer added";
    } else if (height_number == 2) {
      height_demixers.push_front(Tf2ToT2Demixer);
      ABSL_VLOG(1) << "  Tf2ToT2Demixer added";
    }
  }
  demixers.splice(demixers.end(), height_demixers);

  return absl::OkStatus();
}

void ConfigureLabeledFrame(const AudioFrameWithData& audio_frame,
                           LabeledFrame& labeled_frame) {
  labeled_frame.samples_to_trim_at_end =
      audio_frame.obu.header_.num_samples_to_trim_at_end;
  labeled_frame.samples_to_trim_at_start =
      audio_frame.obu.header_.num_samples_to_trim_at_start;
  labeled_frame.demixing_params = audio_frame.down_mixing_params;
}

absl::Span<const std::vector<InternalSampleType>> GetEncodedSamples(
    const AudioFrameWithData& audio_frame_with_data) {
  if (!audio_frame_with_data.encoded_samples.has_value()) {
    return {};
  }
  return absl::MakeConstSpan(*audio_frame_with_data.encoded_samples);
}

absl::Span<const std::vector<InternalSampleType>> GetDecodedSamples(
    const AudioFrameWithData& audio_frame_with_data) {
  return audio_frame_with_data.decoded_samples;
}

absl::Status PassThroughReconGainDataForDecodedAudioFrame(
    const AudioFrameWithData& decoded_audio_frame,
    LabeledFrame& labeled_decoded_frame) {
  if (decoded_audio_frame.audio_element_with_data == nullptr) {
    ABSL_LOG_FIRST_N(INFO, 1)
        << "No audio element with data found, thus layer info is inaccessible.";
    return absl::OkStatus();
  }
  auto layout_config = std::get_if<ScalableChannelLayoutConfig>(
      &decoded_audio_frame.audio_element_with_data->obu.config_);
  if (layout_config == nullptr) {
    ABSL_LOG_IF(INFO, decoded_audio_frame.start_timestamp == 0)
        << "No scalable channel layout config found, thus recon gain "
           "info is not necessary.";
    return absl::OkStatus();
  }
  auto& loudspeaker_layout_per_layer =
      labeled_decoded_frame.loudspeaker_layout_per_layer;
  loudspeaker_layout_per_layer.clear();
  loudspeaker_layout_per_layer.reserve(
      layout_config->channel_audio_layer_configs.size());
  for (const auto& channel_audio_layer_config :
       layout_config->channel_audio_layer_configs) {
    loudspeaker_layout_per_layer.push_back(
        channel_audio_layer_config.loudspeaker_layout);
  }
  labeled_decoded_frame.recon_gain_info_parameter_data =
      decoded_audio_frame.recon_gain_info_parameter_data;
  return absl::OkStatus();
}

absl::Status StoreSamplesForAudioElementId(
    bool use_decoded_samples,
    const std::list<AudioFrameWithData>& audio_frames_or_decoded_audio_frames,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    LabeledFrame& labeled_frame) {
  if (audio_frames_or_decoded_audio_frames.empty()) {
    return absl::OkStatus();
  }
  const InternalTimestamp common_start_timestamp =
      audio_frames_or_decoded_audio_frames.begin()->start_timestamp;

  // All audio frames in the list should have the same number of samples.
  std::optional<size_t> expected_sample_count;
  for (auto& audio_frame : audio_frames_or_decoded_audio_frames) {
    const auto substream_id = audio_frame.obu.GetSubstreamId();
    auto substream_id_labels_iter = substream_id_to_labels.find(substream_id);
    if (substream_id_labels_iter == substream_id_to_labels.end()) {
      // This audio frame might belong to a different audio element; skip it.
      continue;
    }

    // Validate that the frames are all aligned in time.
    RETURN_IF_NOT_OK(CompareTimestamps(common_start_timestamp,
                                       audio_frame.start_timestamp,
                                       "In StoreSamplesForAudioElementId(): "));

    ConfigureLabeledFrame(audio_frame, labeled_frame);
    const auto& labels = substream_id_labels_iter->second;
    const auto input_samples = use_decoded_samples
                                   ? GetDecodedSamples(audio_frame)
                                   : GetEncodedSamples(audio_frame);
    if (input_samples.empty()) {
      return absl::InvalidArgumentError(
          "Input samples are not available for down-mixing.");
    }

    const auto num_channels = labels.size();
    RETURN_IF_NOT_OK(ValidateEqual(
        input_samples.size(), num_channels,
        "Decoded number of channels vs. expected number of channels"));

    absl::StatusOr<size_t> frame_sample_count =
        GetCommonNumTicks(input_samples);
    if (!frame_sample_count.ok()) {
      return frame_sample_count.status();
    }

    if (!expected_sample_count.has_value()) {
      expected_sample_count = *frame_sample_count;
    } else {
      RETURN_IF_NOT_OK(ValidateEqual(*frame_sample_count,
                                     *expected_sample_count,
                                     "Sample count across substreams"));
    }

    int channel_index = 0;
    for (const auto& label : labels) {
      labeled_frame.label_to_samples[label] = input_samples[channel_index];
      channel_index++;
    }
    if (use_decoded_samples) {
      RETURN_IF_NOT_OK(PassThroughReconGainDataForDecodedAudioFrame(
          audio_frame, labeled_frame));
    }
  }

  return absl::OkStatus();
}

absl::Status ApplyDemixers(const std::list<Demixer>& demixers,
                           LabeledFrame& labeled_frame) {
  for (const auto& demixer : demixers) {
    RETURN_IF_NOT_OK(
        demixer(labeled_frame.demixing_params, labeled_frame.label_to_samples));
  }
  return absl::OkStatus();
}

absl::Status GetDemixerMetadata(
    const DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<DecodedUleb128,
                              DemixingMetadataForAudioElementId>&
        audio_element_id_to_demixing_metadata,
    const DemixingMetadataForAudioElementId*& demixing_metadata) {
  const auto iter =
      audio_element_id_to_demixing_metadata.find(audio_element_id);
  if (iter == audio_element_id_to_demixing_metadata.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Demxiing metadata for Audio Element ID= ", audio_element_id,
        " not found"));
  }
  demixing_metadata = &iter->second;
  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
LookupLabelsToReconstruct(const AudioElementObu& obu) {
  switch (obu.GetAudioElementType()) {
    using enum AudioElementObu::AudioElementType;
    case kAudioElementChannelBased: {
      const auto& channel_audio_layer_configs =
          std::get<ScalableChannelLayoutConfig>(obu.config_)
              .channel_audio_layer_configs;
      if (channel_audio_layer_configs.empty()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Expected non-empty channel audio layer configs for Audio "
            "Element ID= ",
            obu.GetAudioElementId()));
      }

      // Reconstruct the highest layer.
      return ChannelLabel::
          LookupLabelsToReconstructFromScalableLoudspeakerLayout(
              channel_audio_layer_configs.back().loudspeaker_layout,
              channel_audio_layer_configs.back().expanded_loudspeaker_layout);
    }
    case kAudioElementSceneBased:
    case kAudioElementObjectBased:
      // OK. Neither Ambisonics nor object-based audio elements have channels to
      // be reconstructed.
      return absl::flat_hash_set<ChannelLabel::Label>{};
    default:
      return absl::UnimplementedError(absl::StrCat(
          "Unsupported audio element type= ", obu.GetAudioElementType()));
  }
}
void LogForAudioElementId(absl::string_view log_prefix,
                          DecodedUleb128 audio_element_id,
                          const IdLabeledFrameMap& id_to_labeled_frame) {
  if (!id_to_labeled_frame.contains(audio_element_id)) {
    return;
  }
  for (const auto& [label, samples] :
       id_to_labeled_frame.at(audio_element_id).label_to_samples) {
    ABSL_VLOG(1) << "  Channel " << label << ":\t" << log_prefix
                 << " frame size= " << samples.size() << ".";
  }
}

}  // namespace

absl::flat_hash_map<DecodedUleb128, DemixingManager::ReconstructionConfig>
DemixingManager::CreateIdToReconstructionConfig(
    const DescriptorObus::AudioElementsById& audio_elements) {
  absl::flat_hash_map<DecodedUleb128, DemixingManager::ReconstructionConfig>
      result;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    result[audio_element_id] = {
        .audio_element_obu = &audio_element_with_data.obu,
        .substream_id_to_labels =
            audio_element_with_data.substream_id_to_labels,
        .label_to_output_gain = audio_element_with_data.label_to_output_gain,
    };
  }
  return result;
}

absl::StatusOr<DemixingManager> DemixingManager::Create(
    const absl::flat_hash_map<DecodedUleb128, ReconstructionConfig>&
        id_to_config) {
  absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>
      audio_element_id_to_demixing_metadata;
  for (const auto& [audio_element_id, reconstruction_config] : id_to_config) {
    ABSL_CHECK_NE(reconstruction_config.audio_element_obu, nullptr);
    const auto labels_to_reconstruct =
        LookupLabelsToReconstruct(*reconstruction_config.audio_element_obu);
    if (!labels_to_reconstruct.ok()) {
      return labels_to_reconstruct.status();
    }

    auto [iter, inserted] = audio_element_id_to_demixing_metadata.insert(
        {audio_element_id, DemixingMetadataForAudioElementId()});
    ABSL_CHECK(inserted)
        << "The target map was initially empty, iterating over "
           "`audio_elements` cannot produce a duplicate key.";
    RETURN_IF_NOT_OK(FillRequiredDemixingMetadata(
        *labels_to_reconstruct, reconstruction_config.substream_id_to_labels,
        reconstruction_config.label_to_output_gain, iter->second));
  }

  return DemixingManager(std::move(audio_element_id_to_demixing_metadata));
}

absl::StatusOr<IdLabeledFrameMap> DemixingManager::DemixOriginalAudioSamples(
    const std::list<AudioFrameWithData>& audio_frames) const {
  IdLabeledFrameMap id_to_labeled_frame;
  for (const auto& [audio_element_id, demixing_metadata] :
       audio_element_id_to_demixing_metadata_) {
    // Process the original audio frames.
    LabeledFrame labeled_frame;
    RETURN_IF_NOT_OK(StoreSamplesForAudioElementId(
        /*use_decoded_samples=*/false, audio_frames,
        demixing_metadata.substream_id_to_labels, labeled_frame));
    if (!labeled_frame.label_to_samples.empty()) {
      RETURN_IF_NOT_OK(
          ApplyDemixers(demixing_metadata.demixers, labeled_frame));
      id_to_labeled_frame[audio_element_id] = std::move(labeled_frame);
    }

    LogForAudioElementId("Original", audio_element_id, id_to_labeled_frame);
  }

  return id_to_labeled_frame;
}

absl::StatusOr<IdLabeledFrameMap> DemixingManager::DemixDecodedAudioSamples(
    const std::list<AudioFrameWithData>& decoded_audio_frames) const {
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  for (const auto& [audio_element_id, demixing_metadata] :
       audio_element_id_to_demixing_metadata_) {
    // Process the decoded audio frames.
    LabeledFrame labeled_decoded_frame;
    RETURN_IF_NOT_OK(StoreSamplesForAudioElementId(
        /*use_decoded_samples=*/true, decoded_audio_frames,
        demixing_metadata.substream_id_to_labels, labeled_decoded_frame));
    if (!labeled_decoded_frame.label_to_samples.empty()) {
      RETURN_IF_NOT_OK(
          ApplyDemixers(demixing_metadata.demixers, labeled_decoded_frame));
      id_to_labeled_decoded_frame[audio_element_id] =
          std::move(labeled_decoded_frame);
    }

    LogForAudioElementId("Decoded", audio_element_id,
                         id_to_labeled_decoded_frame);
  }

  return id_to_labeled_decoded_frame;
}

absl::StatusOr<const std::list<Demixer>* absl_nonnull>
DemixingManager::GetDemixers(DecodedUleb128 audio_element_id) const {
  const DemixingMetadataForAudioElementId* demixing_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDemixerMetadata(audio_element_id,
                                      audio_element_id_to_demixing_metadata_,
                                      demixing_metadata));
  return &demixing_metadata->demixers;
}

}  // namespace iamf_tools
