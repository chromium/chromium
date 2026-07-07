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
#include "iamf/cli/renderer/renderer_utils.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

struct GetChannelLabelsFromAmbisonicsMonoConfig {
  AmbisonicsConfig::AmbisonicsMode mode_;
  const std::vector<DecodedUleb128>& audio_substream_ids_;
  const SubstreamIdLabelsMap& substream_id_to_labels_;
  std::vector<ChannelLabel::Label>& channel_labels_;

  absl::Status operator()(const AmbisonicsMonoConfig& config) {
    if (mode_ != AmbisonicsConfig::kAmbisonicsModeMono) {
      return absl::InvalidArgumentError(
          "Expected mode == `kAmbisonicsModeMono` for AmbisonicsMonoConfig");
    }

    RETURN_IF_NOT_OK(ValidateEqual<size_t>(config.GetSubstreamCount(),
                                           audio_substream_ids_.size(),
                                           "audio_substream_ids_.size()"));
    absl::Span<const uint8_t> channel_mapping = config.GetChannelMappingView();
    channel_labels_.resize(channel_mapping.size());
    for (size_t i = 0; i < channel_labels_.size(); ++i) {
      const uint8_t substream_id_index = channel_mapping[i];
      if (substream_id_index ==
          AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber) {
        // Mixed-order ambisonics representation: this channel is missing.
        channel_labels_[i] = ChannelLabel::kOmitted;
      } else {
        const auto substream_id = audio_substream_ids_[substream_id_index];
        const auto& labels = LookupInMap(substream_id_to_labels_, substream_id,
                                         "Labels for substream ID");
        if (!labels.ok()) {
          return labels.status();
        }

        // For ambisonics mode = MONO, each substream should correspond to
        // only one channel.
        if (labels->size() != 1) {
          return absl::InvalidArgumentError(absl::StrCat(
              "Expected one channel per substream for `kAmbisonicsModeMono`. "
              "substream_id= ",
              substream_id, " contains ", labels->size(), " channels."));
        }
        channel_labels_[i] = labels->front();
      }
    }
    return absl::OkStatus();
  }
};

struct GetChannelLabelsFromAmbisonicsProjectionConfig {
  AmbisonicsConfig::AmbisonicsMode mode_;
  const std::vector<DecodedUleb128>& audio_substream_ids_;
  const SubstreamIdLabelsMap& substream_id_to_labels_;
  std::vector<ChannelLabel::Label>& channel_labels_;

  absl::Status operator()(const AmbisonicsProjectionConfig& config) {
    if (mode_ != AmbisonicsConfig::kAmbisonicsModeProjection) {
      return absl::InvalidArgumentError(
          "Expected mode == `kAmbisonicsModeProjection` for "
          "AmbisonicsProjectionConfig");
    }

    RETURN_IF_NOT_OK(ValidateEqual<size_t>(config.GetSubstreamCount(),
                                           audio_substream_ids_.size(),
                                           "audio_substream_ids_.size()"));
    const int num_channels =
        config.GetSubstreamCount() + config.GetCoupledSubstreamCount();
    channel_labels_.reserve(num_channels);
    for (int i = 0; i < config.GetSubstreamCount(); i++) {
      const auto substream_id = audio_substream_ids_[i];
      const auto& labels = LookupInMap(substream_id_to_labels_, substream_id,
                                       "Labels for substream ID");
      if (!labels.ok()) {
        return labels.status();
      }
      for (const auto& label : *labels) {
        channel_labels_.push_back(label);
      }
    }

    if (channel_labels_.size() != static_cast<size_t>(num_channels)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Inconsistent number of channels. channel_labels->size()= ",
          channel_labels_.size(), " vs num_channels= ", num_channels));
    }

    return absl::OkStatus();
  }
};

double Q15ToSignedDouble(const int16_t input) {
  return static_cast<double>(input) / 32768.0;
}

}  // namespace

absl::StatusOr<std::string> LookupOutputKeyFromPlaybackLayout(
    const Layout& output_layout) {
  switch (output_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention: {
      auto sound_system = std::get<LoudspeakersSsConventionLayout>(
                              output_layout.specific_layout)
                              .sound_system;
      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, std::string>>
          kSoundSystemToOutputKey({
              {kSoundSystemA_0_2_0, "0+2+0"},
              {kSoundSystemB_0_5_0, "0+5+0"},
              {kSoundSystemC_2_5_0, "2+5+0"},
              {kSoundSystemD_4_5_0, "4+5+0"},
              {kSoundSystemE_4_5_1, "4+5+1"},
              {kSoundSystemF_3_7_0, "3+7+0"},
              {kSoundSystemG_4_9_0, "4+9+0"},
              {kSoundSystemH_9_10_3, "9+10+3"},
              {kSoundSystemI_0_7_0, "0+7+0"},
              {kSoundSystemJ_4_7_0, "4+7+0"},
              {kSoundSystem10_2_7_0, "7.1.2"},
              {kSoundSystem11_2_3_0, "3.1.2"},
              {kSoundSystem12_0_1_0, "0+1+0"},
              {kSoundSystem13_6_9_0, "9.1.6"},
              {kSoundSystem14_5_7_4, "7.1.5.4"},
          });

      return LookupInMap(*kSoundSystemToOutputKey, sound_system,
                         "Output key for `SoundSystem`");
    }

    case kLayoutTypeBinaural:
      return absl::UnimplementedError(
          "Loudness layout key for BINAURAL not supported "
          "yet.");
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported layout_type= ", output_layout.layout_type));
  }
}

absl::Status GetAmbisonicsOrder(const uint8_t channel_count, int& order) {
  // IAMF section 3.6.3 (https://aomediacodec.github.io/iamf/#obu-codecconfig)
  // only permits ambisonics orders [0, 14].
  static constexpr int kMaxAmbisonicsOrder = 14;
  for (int i = 0; i <= kMaxAmbisonicsOrder; i++) {
    const int expected_count = (i + 1) * (i + 1);
    if (static_cast<int>(channel_count) == expected_count) {
      order = i;
      return absl::OkStatus();
    } else if (static_cast<int>(channel_count) < expected_count) {
      // Set to upper bound when `IGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE` is
      // defined.
      order = i;
      break;
    }
  }

  return absl::InvalidArgumentError(absl::StrCat(
      channel_count, " is not a valid number of ambisonics channels."));
}

// Helper type for the visitor.
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

absl::Status GetChannelLabelsForAmbisonics(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    std::vector<ChannelLabel::Label>& channel_labels) {
  return std::visit(
      overloaded{GetChannelLabelsFromAmbisonicsMonoConfig{
                     ambisonics_config.GetAmbisonicsMode(), audio_substream_ids,
                     substream_id_to_labels, channel_labels},
                 GetChannelLabelsFromAmbisonicsProjectionConfig{
                     ambisonics_config.GetAmbisonicsMode(), audio_substream_ids,
                     substream_id_to_labels, channel_labels}},
      ambisonics_config.ambisonics_config);
}

absl::Status ProjectSamplesToRender(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    absl::Span<const int16_t> demixing_matrix,
    std::vector<std::vector<InternalSampleType>>& projected_samples) {
  if (input_samples.empty() || demixing_matrix.empty()) {
    return absl::InvalidArgumentError(
        "Input samples or demixing matrix is empty.");
  }

  const auto num_input_channels = input_samples.size();
  const auto num_ticks = input_samples[0].size();

  const int num_elements_in_demixing_matrix = demixing_matrix.size();
  if (num_elements_in_demixing_matrix % num_input_channels != 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Number of elements in the demixing matrix= ",
                     num_elements_in_demixing_matrix,
                     " is not a multiple of number of input channels= ",
                     num_input_channels));
  }

  const int num_output_channels =
      num_elements_in_demixing_matrix / num_input_channels;
  projected_samples.resize(num_output_channels);
  for (int out_channel = 0; out_channel < num_output_channels; out_channel++) {
    auto& projected_samples_for_channel = projected_samples[out_channel];
    projected_samples_for_channel.assign(num_ticks, 0.0);
    for (size_t in_channel = 0; in_channel < num_input_channels; in_channel++) {
      const auto& input_sample_for_channel = input_samples[in_channel];
      const auto demixing_value = Q15ToSignedDouble(
          demixing_matrix[in_channel * num_output_channels + out_channel]);

      for (size_t t = 0; t < num_ticks; t++) {
        // Project with `demixing_matrix`, which is encoded as Q15 and stored
        // in column major.
        projected_samples_for_channel[t] +=
            demixing_value * input_sample_for_channel[t];
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
