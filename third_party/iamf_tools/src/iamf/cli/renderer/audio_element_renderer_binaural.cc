
/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/renderer/audio_element_renderer_binaural.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/types.h"
#include "obr/audio_buffer/audio_buffer.h"
#include "obr/renderer/audio_element_type.h"
#include "obr/renderer/obr_impl.h"

namespace iamf_tools {
namespace {

constexpr size_t kNumBinauralChannels = 2;
constexpr size_t kObrMinFftSize = 32;
constexpr size_t kObrMaxSupportedNumFrames = 16384;

absl::StatusOr<obr::AudioElementType>
LookupObrAudioElementTypeFromLoudspeakerLayout(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout) {
  if (loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "expanded_loudspeaker_layout"));
    // TODO(b/450471766): Support expanded loudspeaker layout.
    return absl::UnimplementedError(
        "Expanded loudspeaker layout not supported yet");
  }

  using enum ChannelAudioLayerConfig::LoudspeakerLayout;
  static const absl::NoDestructor<absl::flat_hash_map<
      ChannelAudioLayerConfig::LoudspeakerLayout, obr::AudioElementType>>
      kLoudspeakerLayoutToObrAudioElementType({
          {kLayoutMono, obr::AudioElementType::kLayoutMono},
          {kLayoutStereo, obr::AudioElementType::kLayoutStereo},
          {kLayout5_1_ch, obr::AudioElementType::kLayout5_1_0_ch},
          {kLayout5_1_2_ch, obr::AudioElementType::kLayout5_1_2_ch},
          {kLayout5_1_4_ch, obr::AudioElementType::kLayout5_1_4_ch},
          {kLayout7_1_ch, obr::AudioElementType::kLayout7_1_0_ch},
          {kLayout7_1_2_ch, obr::AudioElementType::kLayout7_1_2_ch},
          {kLayout7_1_4_ch, obr::AudioElementType::kLayout7_1_4_ch},
          {kLayout3_1_2_ch, obr::AudioElementType::kLayout3_1_2_ch},
      });

  return LookupInMap(*kLoudspeakerLayoutToObrAudioElementType,
                     loudspeaker_layout,
                     "OBR's Audio Element Type for `LoudspeakerLayout`");
}

absl::StatusOr<obr::AudioElementType>
GetObrAudioElementTypeFromAmbisonicsConfig(
    const AmbisonicsConfig& ambisonics_config) {
  const auto output_channel_count = ambisonics_config.GetOutputChannelCount();

  int ambisonics_order = 0;
  RETURN_IF_NOT_OK(GetAmbisonicsOrder(output_channel_count, ambisonics_order));

  // TODO(b/459993192): OBR to support 0-th order ambisonics.
  static const absl::NoDestructor<
      absl::flat_hash_map<int, obr::AudioElementType>>
      kAmbisoncisOrderToObrAudioElementType({
          {1, obr::AudioElementType::k1OA},
          {2, obr::AudioElementType::k2OA},
          {3, obr::AudioElementType::k3OA},
          {4, obr::AudioElementType::k4OA},
      });

  return LookupInMap(*kAmbisoncisOrderToObrAudioElementType, ambisonics_order,
                     "OBR's Audio Element Type for ambisonics order");
}

template <bool ResizeDestinationChannels, class DestinationValueType,
          class SourceBufferType, class DestinationBufferType>
void CopySamples(size_t num_channels, const SourceBufferType& source_buffer,
                 DestinationBufferType& destination_buffer,
                 size_t num_destination_channels_samples = 0) {
  if constexpr (ResizeDestinationChannels) {
    if (num_destination_channels_samples > source_buffer[0].size()) {
      ABSL_LOG(ERROR) << "Destination buffer size is too small. "
                      << num_destination_channels_samples << " vs "
                      << source_buffer[0].size();
      return;
    }
  } else {
    num_destination_channels_samples = source_buffer[0].size();
  }

  for (size_t c = 0; c < num_channels; c++) {
    const auto& source_samples_for_channel = source_buffer[c];
    auto& destination_buffer_for_channel = destination_buffer[c];

    // Resize destination channels. This step may be skipped if the
    // destination buffer is already pre-allocated.
    if constexpr (ResizeDestinationChannels) {
      destination_buffer_for_channel.resize(num_destination_channels_samples);
    }

    for (size_t t = 0; t < num_destination_channels_samples; t++) {
      destination_buffer_for_channel[t] =
          static_cast<DestinationValueType>(source_samples_for_channel[t]);
    }
  }
}

absl::Status ValidateObrArguments(size_t num_samples_per_frame,
                                  size_t sample_rate) {
  if (num_samples_per_frame < kObrMinFftSize) {
    return absl::InvalidArgumentError(absl::StrCat(
        "OBR does not support `num_samples_per_frame` less than ",
        kObrMinFftSize, " samples. (got ", num_samples_per_frame, ")"));
  }

  if (num_samples_per_frame > kObrMaxSupportedNumFrames) {
    return absl::InvalidArgumentError(
        absl::StrCat("Only frame lengths up to ", kObrMaxSupportedNumFrames,
                     " are supported. (got ", num_samples_per_frame, ")"));
  }

  if (sample_rate <= 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Sampling rate must be greater than 0. (got ", sample_rate, ")"));
  }

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<AudioElementRendererBinaural>
AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    size_t num_samples_per_frame, size_t sample_rate) {
  if (const auto status =
          ValidateObrArguments(num_samples_per_frame, sample_rate);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  auto obr = std::make_unique<obr::ObrImpl>(
      static_cast<int>(num_samples_per_frame), static_cast<int>(sample_rate));

  const auto& highest_channel_audio_layer_config =
      scalable_channel_layout_config.channel_audio_layer_configs.back();
  const auto& obr_audio_element_type =
      LookupObrAudioElementTypeFromLoudspeakerLayout(
          highest_channel_audio_layer_config.loudspeaker_layout,
          highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!obr_audio_element_type.ok()) {
    ABSL_LOG(ERROR) << obr_audio_element_type.status();
    return nullptr;
  }

  auto add_audio_element_status = obr->AddAudioElement(*obr_audio_element_type);
  if (!add_audio_element_status.ok()) {
    ABSL_LOG(ERROR) << add_audio_element_status;
    return nullptr;
  }

  const auto& ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          highest_channel_audio_layer_config.loudspeaker_layout,
          highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!ordered_labels.ok()) {
    ABSL_LOG(ERROR) << ordered_labels.status();
    return nullptr;
  }
  return absl::WrapUnique(new AudioElementRendererBinaural(
      *ordered_labels, /*demixing_matrix=*/std::nullopt, std::move(obr),
      num_samples_per_frame));
}

std::unique_ptr<AudioElementRendererBinaural>
AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    size_t num_samples_per_frame, size_t sample_rate) {
  if (const auto status =
          ValidateObrArguments(num_samples_per_frame, sample_rate);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  auto obr = std::make_unique<obr::ObrImpl>(
      static_cast<int>(num_samples_per_frame), static_cast<int>(sample_rate));

  const auto& obr_audio_element_type =
      GetObrAudioElementTypeFromAmbisonicsConfig(ambisonics_config);
  if (!obr_audio_element_type.ok()) {
    ABSL_LOG(ERROR) << obr_audio_element_type.status();
    return nullptr;
  }

  auto add_audio_element_status = obr->AddAudioElement(*obr_audio_element_type);
  if (!add_audio_element_status.ok()) {
    ABSL_LOG(ERROR) << add_audio_element_status;
    return nullptr;
  }

  // Create `ordered_labels`.
  std::vector<ChannelLabel::Label> ordered_labels;
  const auto status =
      GetChannelLabelsForAmbisonics(ambisonics_config, audio_substream_ids,
                                    substream_id_to_labels, ordered_labels);
  if (!status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererBinaural(
      ordered_labels, ambisonics_config.GetDemixingMatrix(), std::move(obr),
      num_samples_per_frame));
}

AudioElementRendererBinaural::AudioElementRendererBinaural(
    const std::vector<ChannelLabel::Label>& ordered_labels,
    std::optional<absl::Span<const int16_t>> demixing_matrix,
    std::unique_ptr<obr::ObrImpl> obr, size_t num_samples_per_frame)
    : AudioElementRendererBase(ordered_labels, num_samples_per_frame,
                               /*num_output_channels=*/kNumBinauralChannels),
      obr_(std::move(obr)),
      input_buffer_(
          // Input may be projected using the demixing matrix.
          demixing_matrix.has_value()
              ? demixing_matrix->size() / ordered_labels.size()
              : ordered_labels.size(),
          num_samples_per_frame_),
      output_buffer_(num_output_channels_, num_samples_per_frame_),
      demixing_matrix_(
          demixing_matrix.has_value()
              ? std::make_optional(std::vector<int16_t>(
                    demixing_matrix->begin(), demixing_matrix->end()))
              : std::nullopt) {}

absl::Status AudioElementRendererBinaural::RenderSamples(
    absl::Span<const absl::Span<const InternalSampleType>> samples_to_render) {
  // Some sanity checks.
  ABSL_DCHECK_EQ(rendered_samples_.size(), obr_->GetNumberOfOutputChannels());

  // Copy samples to the input audio buffer; optionally project the input
  // samples first.
  if (demixing_matrix_.has_value()) {
    RETURN_IF_NOT_OK(ProjectSamplesToRender(
        samples_to_render, *demixing_matrix_, projected_samples_));

    // Check that the input shape to OBR is as expected.
    ABSL_DCHECK_EQ(projected_samples_.size(), obr_->GetNumberOfInputChannels());

    CopySamples<false, ObrSampleType>(projected_samples_.size(),
                                      projected_samples_, input_buffer_);
  } else {
    // Check that the input shape to OBR is as expected.
    ABSL_DCHECK_EQ(samples_to_render.size(), obr_->GetNumberOfInputChannels());

    CopySamples<false, ObrSampleType>(samples_to_render.size(),
                                      samples_to_render, input_buffer_);
  }

  // TODO(b/461969607): Mitigate mutants by mocking `ObrImpl` and verifying
  //                    arguments to improve test coverage.
  // Process.
  obr_->Process(input_buffer_, &output_buffer_);

  // Copy samples from the output audio buffer to `rendered_samples_`.
  CopySamples<true, InternalSampleType>(kNumBinauralChannels, output_buffer_,
                                        rendered_samples_,
                                        samples_to_render[0].size());

  return absl::OkStatus();
}

}  // namespace iamf_tools
