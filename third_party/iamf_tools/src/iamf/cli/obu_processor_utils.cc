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
#include "iamf/cli/obu_processor_utils.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/rendering_config.h"

namespace iamf_tools {

namespace {

using LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0;

template <typename Condition>
MixPresentationObu* FindMixPresentationWithCondition(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    Condition condition) {
  for (const auto candidate_mix : supported_mix_presentations) {
    if (condition(*candidate_mix)) {
      return candidate_mix;
    }
  }
  return nullptr;
}

bool IsLayoutStereo(const Layout& layout) {
  const auto* ss_layout =
      std::get_if<LoudspeakersSsConventionLayout>(&layout.specific_layout);
  return ss_layout != nullptr && ss_layout->sound_system == kSoundSystemA_0_2_0;
}

bool IsLayoutBinaural(const Layout& layout) {
  return layout.layout_type == Layout::kLayoutTypeBinaural;
}

// Returns true if the sub-mix has exactly one audio element and that element
// has the last layer as the specified loudspeaker layout.
bool HasOneAudioElementWithLayout(
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const DescriptorObus::AudioElementsById& audio_elements,
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout) {
  if (sub_mix_audio_elements.size() != 1) {
    return false;
  }

  auto it =
      audio_elements.find(sub_mix_audio_elements.front().audio_element_id);
  if (it == audio_elements.end()) {
    return false;
  }

  const auto* scalable_channel_layout_config =
      std::get_if<ScalableChannelLayoutConfig>(&it->second.obu.config_);
  if (scalable_channel_layout_config == nullptr) {
    return false;
  }

  // Check the last layer, which will be the highest layer if there is
  // multi-layer audio.
  return scalable_channel_layout_config->channel_audio_layer_configs.back()
             .loudspeaker_layout == loudspeaker_layout;
}

bool MixPresentationContainsLayout(const MixPresentationObu& candidate_mix,
                                   const Layout& desired_layout) {
  for (const auto& sub_mix : candidate_mix.sub_mixes_) {
    for (const auto& layout : sub_mix.layouts) {
      if (layout.loudness_layout == desired_layout) {
        return true;
      }
    }
  }
  return false;
}

bool HasOneStereoLayoutAndAudioElement(
    const MixPresentationObu& mix_presentation,
    const DescriptorObus::AudioElementsById& audio_elements) {
  if (mix_presentation.sub_mixes_.empty()) {
    return false;
  }
  // The selection logic in the spec is silent about multiple sub-mixes.
  // This implementation takes the liberty here to determine the preferred
  // stereo layout, based on the first sub-mix. This will preserve the
  // selection if there is an additional "system sound" sub-mix added on
  // top of the original content.
  const auto& first_submix = mix_presentation.sub_mixes_.front();

  return first_submix.layouts.size() == 1 &&
         IsLayoutStereo(first_submix.layouts.front().loudness_layout) &&
         HasOneAudioElementWithLayout(first_submix.audio_elements,
                                      audio_elements,
                                      ChannelAudioLayerConfig::kLayoutStereo);
}

MixPresentationObu* FindPreferredStereoLoudspeakerMixPresentation(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const DescriptorObus::AudioElementsById& audio_elements) {
  // 2.2.1: Select the first matching mix with all of the following true:
  //        - Exactly one stereo loudness layout.
  //        - Exactly one stereo audio element.
  //        - `headphones_rendering_mode == kHeadphonesRenderingModeStereo`.
  auto* candidate_mix = FindMixPresentationWithCondition(
      supported_mix_presentations,
      [&](const MixPresentationObu& candidate_mix) {
        return HasOneStereoLayoutAndAudioElement(candidate_mix,
                                                 audio_elements) &&
               candidate_mix.sub_mixes_.front()
                       .audio_elements.front()
                       .rendering_config.headphones_rendering_mode ==
                   RenderingConfig::HeadphonesRenderingMode::
                       kHeadphonesRenderingModeStereo;
      });
  if (candidate_mix != nullptr) {
    return candidate_mix;
  }

  // 2.2.2: Similar to clause 2.2.1, but relaxing the constraint on
  //        `headphones_rendering_mode`.
  return FindMixPresentationWithCondition(
      supported_mix_presentations,
      [&](const MixPresentationObu& candidate_mix) {
        return HasOneStereoLayoutAndAudioElement(candidate_mix, audio_elements);
      });
}

MixPresentationObu* FindPreferredBinauralMixPresentation(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const DescriptorObus::AudioElementsById& audio_elements) {
  // 2.1.1: Select the mix with exactly one audio_element_id where its
  //        loudspeaker_layout is BINAURAL.
  auto* candidate_mix = FindMixPresentationWithCondition(
      supported_mix_presentations,
      [&](const MixPresentationObu& candidate_mix) {
        if (candidate_mix.sub_mixes_.empty()) {
          return false;
        }
        return HasOneAudioElementWithLayout(
            candidate_mix.sub_mixes_.front().audio_elements, audio_elements,
            ChannelAudioLayerConfig::kLayoutBinaural);
      });
  if (candidate_mix != nullptr) {
    return candidate_mix;
  }

  // 2.1.2: If there is no such mix, select the mix with the layout_type field
  //        in loudness_layout = BINAURAL.
  return FindMixPresentationWithCondition(
      supported_mix_presentations,
      [&](const MixPresentationObu& candidate_mix) {
        for (const auto& sub_mix : candidate_mix.sub_mixes_) {
          for (const auto& layout : sub_mix.layouts) {
            if (IsLayoutBinaural(layout.loudness_layout)) {
              return true;
            }
          }
        }
        return false;
      });
}

}  // namespace

// TODO(b/438176780): Ensure this is conformant to IAMF spec §7.4.1.
// TODO(b/438178739): Find a different way of rendering requested layouts not in
// the bitstream.
absl::StatusOr<SelectedMixPresentation> FindMixPresentationAndLayout(
    const DescriptorObus::AudioElementsById& audio_elements,
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const std::optional<Layout>& desired_layout,
    std::optional<uint32_t> desired_mix_presentation_id) {
  if (supported_mix_presentations.empty()) {
    return absl::InvalidArgumentError("No supported mix presentations found.");
  }

  MixPresentationObu* mix_presentation = nullptr;
  // 1: If given an ID first try to find a matching MixPresentation.
  if (desired_mix_presentation_id.has_value()) {
    mix_presentation = FindMixPresentationWithCondition(
        supported_mix_presentations,
        [&desired_mix_presentation_id](
            const MixPresentationObu& candidate_mix) {
          return candidate_mix.GetMixPresentationId() ==
                 *desired_mix_presentation_id;
        });
  }

  // 2: If not given an ID or not found by ID, find by given layout.
  if (mix_presentation == nullptr && desired_layout.has_value()) {
    if (IsLayoutBinaural(*desired_layout)) {
      // 2.1.1 and 2.1.2: Special cases when the output is binaural.
      mix_presentation = FindPreferredBinauralMixPresentation(
          supported_mix_presentations, audio_elements);
      // TODO(b/438176780): Implement 2.1.3. Fallback to the "highest" layout.
    } else if (IsLayoutStereo(*desired_layout)) {
      // 2.2.1 and 2.2.2: Special cases when the output is stereo.
      mix_presentation = FindPreferredStereoLoudspeakerMixPresentation(
          supported_mix_presentations, audio_elements);
      // TODO(b/438176780): Implement 2.2.3. Fallback to the "highest" layout.
    } else {
      // 2.3.1: Select the first matching mix with the desired layout.
      mix_presentation = FindMixPresentationWithCondition(
          supported_mix_presentations,
          [&desired_layout](const MixPresentationObu& candidate_mix) {
            return MixPresentationContainsLayout(candidate_mix,
                                                 *desired_layout);
          });
      // TODO(b/438176780): Implement 2.3.2. Fallback to the "highest" layout.
    }
  }

  // 3: By this step if we don't have a MixPresentation, take first.
  if (mix_presentation == nullptr) {
    mix_presentation = supported_mix_presentations.front();
  }
  // Check that the selected Mix has at least one sub-mix.
  if (mix_presentation->sub_mixes_.empty()) {
    return absl::InvalidArgumentError(
        "No submixes found in the selected mix presentation.");
  }

  // 4: Find an output layout either from desired layout or default.
  if (!desired_layout.has_value()) {
    // A: If no desired layout default to the first submix's layout.
    if (mix_presentation->sub_mixes_.front().layouts.empty()) {
      return absl::InvalidArgumentError(
          "No layouts found in the first submix of the first mix "
          "presentation.");
    }
    return SelectedMixPresentation{
        mix_presentation,
        mix_presentation->sub_mixes_.front().layouts.front().loudness_layout,
        0,
        0,
    };
  }

  // B. Desired layout specified: Search for it in the selected Mix.
  for (size_t sub_mix_index = 0;
       sub_mix_index < mix_presentation->sub_mixes_.size(); ++sub_mix_index) {
    const auto& sub_mix = mix_presentation->sub_mixes_[sub_mix_index];
    for (size_t layout_index = 0; layout_index < sub_mix.layouts.size();
         ++layout_index) {
      const auto& layout = sub_mix.layouts[layout_index];
      if (desired_layout == layout.loudness_layout) {
        return SelectedMixPresentation{
            mix_presentation,
            layout.loudness_layout,
            static_cast<int>(sub_mix_index),
            static_cast<int>(layout_index),
        };
      }
    }
  }

  // C: Desired layout not found in the Mix so add it to the first sub-mix.
  mix_presentation->sub_mixes_.front().layouts.push_back(
      {.loudness_layout = *desired_layout});
  return SelectedMixPresentation{
      mix_presentation,
      *desired_layout,
      0,
      static_cast<int>(mix_presentation->sub_mixes_.front().layouts.size()) - 1,
  };
}

absl::StatusOr<MixPresentationObu> CreateSimplifiedMixPresentationForRendering(
    const MixPresentationObu& mix_presentation, int sub_mix_index,
    int layout_index) {
  if (sub_mix_index < 0 || static_cast<size_t>(sub_mix_index) >=
                               mix_presentation.sub_mixes_.size()) {
    return absl::OutOfRangeError(absl::StrCat(
        "Sub-mix index is out of bounds for the given Mix Presentation: ",
        mix_presentation.sub_mixes_.size(), " sub_mix_index: ", sub_mix_index));
  }
  const auto& selected_sub_mix = mix_presentation.sub_mixes_[sub_mix_index];
  if (layout_index < 0 ||
      static_cast<size_t>(layout_index) >= selected_sub_mix.layouts.size()) {
    return absl::OutOfRangeError(
        "Layout index is out of bounds for the given Mix Presentation.");
  }
  const auto& selected_layout = selected_sub_mix.layouts[layout_index];

  // Clone the mix presentation, keep only the selected sub-mix and layout.
  MixPresentationObu simplified_mix_presentation = mix_presentation;
  simplified_mix_presentation.sub_mixes_ = {selected_sub_mix};
  simplified_mix_presentation.sub_mixes_[0].layouts = {selected_layout};

  return simplified_mix_presentation;
}

}  // namespace iamf_tools
