
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

#include "iamf/cli/renderer/audio_element_renderer_channel_to_channel.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/log/absl_vlog_is_on.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/loudspeakers_renderer.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr absl::string_view kMonoInputKey = "0+1+0";
constexpr absl::string_view kStereoInputKey = "0+2+0";
constexpr absl::string_view k5_1_chInputKey = "0+5+0";
constexpr absl::string_view k5_1_2_chInputKey = "2+5+0";
constexpr absl::string_view k5_1_4InputKey = "4+5+0";
constexpr absl::string_view k7_1_0InputKey = "0+7+0";
constexpr absl::string_view k7_1_4InputKey = "4+7+0";
constexpr absl::string_view k7_1_2InputKey = "7.1.2";
constexpr absl::string_view k3_1_2InputKey = "3.1.2";
constexpr absl::string_view k9_1_6InputKey = "9.1.6";
constexpr absl::string_view k10_2_9_3InputKey = "9+10+3";
constexpr absl::string_view k7_1_5_4InputKey = "7.1.5.4";

// TODO(b/359180486): Unify with `IsExpandedLayoutEquivalentToSoundSystem` in
//                    `audio_element_passthrough.cc`.
absl::StatusOr<absl::string_view> LookupInputKeyFromLoudspeakerLayout(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  switch (expanded_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kExpandedLayoutStereoS:
      return k5_1_4InputKey;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
      return k7_1_4InputKey;
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      return k9_1_6InputKey;
    case kExpandedLayout10_2_9_3:
    case kExpandedLayoutLfePair:
    case kExpandedLayoutBottom3Ch:
      return k10_2_9_3InputKey;
    case kExpandedLayout7_1_5_4Ch:
    case kExpandedLayoutBottom4Ch:
    case kExpandedLayoutTop1Ch:
    case kExpandedLayoutTop5Ch:
      return k7_1_5_4InputKey;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Channel order not found for layout= ", expanded_layout));
  }
}

absl::StatusOr<absl::string_view> LookupInputKeyFromLoudspeakerLayout(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout) {
  if (loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "expanded_loudspeaker_layout"));
    return LookupInputKeyFromLoudspeakerLayout(*expanded_loudspeaker_layout);
  }

  using enum LoudspeakersSsConventionLayout::SoundSystem;
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  static const absl::NoDestructor<absl::flat_hash_map<
      ChannelAudioLayerConfig::LoudspeakerLayout, absl::string_view>>
      kLoudspeakerLayoutToInputKey({
          {kLayoutMono, kMonoInputKey},
          {kLayoutStereo, kStereoInputKey},
          {kLayout5_1_ch, k5_1_chInputKey},
          {kLayout5_1_2_ch, k5_1_2_chInputKey},
          {kLayout5_1_4_ch, k5_1_4InputKey},
          {kLayout7_1_ch, k7_1_0InputKey},
          {kLayout7_1_4_ch, k7_1_4InputKey},
          {kLayout7_1_2_ch, k7_1_2InputKey},
          {kLayout3_1_2_ch, k3_1_2InputKey},
      });

  return LookupInMap(*kLoudspeakerLayoutToInputKey, loudspeaker_layout,
                     "Input key for `LoudspeakerLayout`");
}

void PrintGainsForDebugging(
    const std::vector<ChannelLabel::Label>& channel_labels_for_debugging,
    const std::vector<std::vector<double>>& gains) {
  auto fmt = std::setw(7);
  std::stringstream ss;
  for (const auto& label : channel_labels_for_debugging) {
    ss << fmt << absl::StrCat(label);
  }
  ABSL_LOG_FIRST_N(INFO, 5) << ss.str();
  for (size_t i = 0; i < gains.front().size(); i++) {
    ss.str({});
    ss.clear();
    ss << std::setprecision(3);
    for (size_t j = 0; j < gains.size(); j++) {
      ss << fmt << gains.at(j).at(i);
    }
    ABSL_LOG_FIRST_N(INFO, 5) << ss.str();
  }
}

}  // namespace

std::unique_ptr<AudioElementRendererChannelToChannel>
AudioElementRendererChannelToChannel::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout, size_t num_samples_per_frame,
    const TrimmingSettings trimming_settings) {
  if (scalable_channel_layout_config.channel_audio_layer_configs.empty()) {
    ABSL_LOG(ERROR) << "No channel audio layer configs provided.";
    return nullptr;
  }
  const auto& highest_channel_audio_layer_config =
      scalable_channel_layout_config.channel_audio_layer_configs.back();
  const auto& ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          highest_channel_audio_layer_config.loudspeaker_layout,
          highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!ordered_labels.ok()) {
    ABSL_LOG(ERROR) << ordered_labels.status();
    return nullptr;
  }

  const auto& input_key = LookupInputKeyFromLoudspeakerLayout(
      highest_channel_audio_layer_config.loudspeaker_layout,
      highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!input_key.ok()) {
    ABSL_LOG(ERROR) << input_key.status();
    return nullptr;
  }
  const auto& output_key = LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    ABSL_LOG(ERROR) << output_key.status();
    return nullptr;
  }

  const auto& gains = LookupPrecomputedGains(*input_key, *output_key);
  if (!gains.ok()) {
    ABSL_LOG(ERROR) << gains.status();
    return nullptr;
  }

  int32_t num_output_channels = 0;
  const auto& num_output_channels_status =
      MixPresentationObu::GetNumChannelsFromLayout(playback_layout,
                                                   num_output_channels);
  if (!num_output_channels_status.ok()) {
    ABSL_LOG(ERROR) << num_output_channels_status;
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererChannelToChannel(
      *input_key, *output_key, static_cast<size_t>(num_output_channels),
      num_samples_per_frame, trimming_settings, *ordered_labels, *gains));
}

absl::Status AudioElementRendererChannelToChannel::RenderSamples(
    absl::Span<const absl::Span<const InternalSampleType>> samples_to_render) {
  // When the demixing parameters are in the bitstream, recompute for every
  // frame and do not store the result in the map.
  // TODO(b/292174366): Find a better solution and strictly follow the spec for
  //                    which renderer to use.
  auto newly_computed_gains = MaybeComputeDynamicGains(
      current_labeled_frame_->demixing_params, input_key_, output_key_);
  if (newly_computed_gains.has_value() && ABSL_VLOG_IS_ON(1)) {
    PrintGainsForDebugging(ordered_labels_, *newly_computed_gains);
  }

  // Dynamic gains may not be relevant, signalled, or known. Fallback to the
  // precomputed gains, to allow rendering to proceed.
  const std::vector<std::vector<double>>& gains_to_use =
      newly_computed_gains.has_value() ? *newly_computed_gains : gains_;

  // Render the samples.
  return RenderChannelLayoutToLoudspeakers(samples_to_render, gains_to_use,
                                           rendered_samples_);
}

}  // namespace iamf_tools
