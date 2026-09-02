
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

#include "iamf/cli/renderer/audio_element_renderer_passthrough.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum LoudspeakersSsConventionLayout::SoundSystem;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;

absl::StatusOr<bool> IsLoudspeakerLayoutEquivalentToSoundSystem(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    LoudspeakersSsConventionLayout::SoundSystem layout) {
  static const absl::NoDestructor<
      absl::flat_hash_map<LoudspeakersSsConventionLayout::SoundSystem,
                          ChannelAudioLayerConfig::LoudspeakerLayout>>
      kSoundSystemToLoudspeakerLayout({
          {kSoundSystem12_0_1_0, kLayoutMono},
          {kSoundSystemA_0_2_0, kLayoutStereo},
          {kSoundSystemB_0_5_0, kLayout5_1_ch},
          {kSoundSystemC_2_5_0, kLayout5_1_2_ch},
          {kSoundSystemD_4_5_0, kLayout5_1_4_ch},
          {kSoundSystemI_0_7_0, kLayout7_1_ch},
          {kSoundSystem10_2_7_0, kLayout7_1_2_ch},
          {kSoundSystemJ_4_7_0, kLayout7_1_4_ch},
          {kSoundSystem11_2_3_0, kLayout3_1_2_ch},
      });

  ChannelAudioLayerConfig::LoudspeakerLayout equivalent_loudspeaker_layout;
  RETURN_IF_NOT_OK(
      CopyFromMap(*kSoundSystemToLoudspeakerLayout, layout,
                  "`LoudspeakerLayout` equivalent to `SoundSystem`",
                  equivalent_loudspeaker_layout));

  return equivalent_loudspeaker_layout == loudspeaker_layout;
}

// Several expanded layouts are defined as being based on a particular sound
// system. The passthrough renderer can be used with the associated sound system
// if the expanded layout is a based on the sound system. Other channels can be
// omitted.
absl::StatusOr<bool> IsExpandedLoudspeakerLayoutBasedOnSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout
        expanded_loudspeaker_layout,
    LoudspeakersSsConventionLayout::SoundSystem layout) {
  switch (expanded_loudspeaker_layout) {
    case kExpandedLayoutStereoS:
      return layout == kSoundSystemD_4_5_0;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
      return layout == kSoundSystemJ_4_7_0;
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      return layout == kSoundSystem13_6_9_0;
    case kExpandedLayout10_2_9_3:
    case kExpandedLayoutLfePair:
    case kExpandedLayoutBottom3Ch:
      return layout == kSoundSystemH_9_10_3;
    case kExpandedLayout7_1_5_4Ch:
    case kExpandedLayoutBottom4Ch:
    case kExpandedLayoutTop1Ch:
    case kExpandedLayoutTop5Ch:
      return layout == kSoundSystem14_5_7_4;
    case kExpandedLayoutReserved20:
    case kExpandedLayoutReserved255:
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown expanded layout cannot be used for pass-through: ",
          expanded_loudspeaker_layout));
  }
}

absl::StatusOr<bool> CanChannelAudioLayerConfigPassThroughToLayout(
    const ChannelAudioLayerConfig& channel_config, const Layout& layout) {
  switch (layout.layout_type) {
    case Layout::kLayoutTypeLoudspeakersSsConvention:
      // Allow Binaural input to pass-through to Stereo (2.0) output.
      if (channel_config.loudspeaker_layout == kLayoutBinaural &&
          std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
                  .sound_system ==
              LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0) {
        return true;
      }
      // Pass-through the associated demixed layer.
      if (channel_config.loudspeaker_layout == kLayoutExpanded) {
        RETURN_IF_NOT_OK(
            ValidateHasValue(channel_config.expanded_loudspeaker_layout,
                             "expanded_loudspeaker_layout"));
        return IsExpandedLoudspeakerLayoutBasedOnSoundSystem(
            *channel_config.expanded_loudspeaker_layout,
            std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
                .sound_system);
      }
      return IsLoudspeakerLayoutEquivalentToSoundSystem(
          channel_config.loudspeaker_layout,
          std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
              .sound_system);
    case Layout::kLayoutTypeBinaural:
      // Pass-through binaural.
      return channel_config.loudspeaker_layout == kLayoutBinaural;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown layout_type= ", layout.layout_type));
  }
}

// Finds the layer with the equivalent loudspeaker layout if present.
absl::StatusOr<ChannelAudioLayerConfig> FindEquivalentLayer(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& layout) {
  for (const auto& channel_audio_layer_config :
       scalable_channel_layout_config.channel_audio_layer_configs) {
    const auto can_pass_through = CanChannelAudioLayerConfigPassThroughToLayout(
        channel_audio_layer_config, layout);
    if (!can_pass_through.ok()) {
      return can_pass_through.status();
    } else if (*can_pass_through) {
      return channel_audio_layer_config;
    } else {
      // Search in the remaining layers.
      continue;
    }
  }

  return absl::InvalidArgumentError(
      "No equivalent layers found for the requested layout. The passthrough "
      "render is not suitable here. Down-mixing may be required.");
}

}  // namespace

std::unique_ptr<AudioElementRendererPassThrough>
AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout, size_t num_samples_per_frame,
    const TrimmingSettings trimming_settings) {
  const auto& equivalent_layer =
      FindEquivalentLayer(scalable_channel_layout_config, playback_layout);
  if (!equivalent_layer.ok()) {
    return nullptr;
  }
  const auto& ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          equivalent_layer->loudspeaker_layout,
          equivalent_layer->expanded_loudspeaker_layout);
  if (!ordered_labels.ok()) {
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererPassThrough(
      *ordered_labels, num_samples_per_frame, trimming_settings));
}

absl::Status AudioElementRendererPassThrough::RenderSamples(
    absl::Span<const absl::Span<const InternalSampleType>> samples_to_render) {
  rendered_samples_.resize(samples_to_render.size());
  for (size_t c = 0; c < samples_to_render.size(); c++) {
    rendered_samples_[c].insert(rendered_samples_[c].end(),
                                samples_to_render[c].begin(),
                                samples_to_render[c].end());
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
