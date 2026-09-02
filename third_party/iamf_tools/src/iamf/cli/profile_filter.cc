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
#include "iamf/cli/profile_filter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ClearAndReturnError(
    absl::string_view context,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  profile_versions.clear();
  return absl::InvalidArgumentError(context);
}

absl::Status FilterAudioElementType(
    absl::string_view debugging_context,
    AudioElementObu::AudioElementType audio_element_type,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  switch (audio_element_type) {
    using enum AudioElementObu::AudioElementType;
    case AudioElementObu::kAudioElementChannelBased:
    case AudioElementObu::kAudioElementSceneBased:
      break;
    case AudioElementObu::kAudioElementObjectBased:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      break;
    default:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseAdvancedProfile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced1Profile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced2Profile);
      break;
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has audio element type= ", audio_element_type,
        ". But the requested profiles do support not support this type."));
  }
  return absl::OkStatus();
}

// Filters out profiles that do not support specific expanded loudspeaker
// layouts. This function assumes profiles that do not support expanded layout
// have already been filtered out (e.g. kIamfSimpleProfile, kIamfBaseProfile).
absl::Status FilterExpandedLoudspeakerLayout(
    absl::string_view debugging_context,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (auto status = ValidateHasValue(expanded_loudspeaker_layout,
                                     "expanded_loudspeaker_layout");
      !status.ok()) {
    return ClearAndReturnError(
        absl::StrCat(debugging_context, status.message()), profile_versions);
  }

  switch (*expanded_loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoS:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      break;
    case kExpandedLayout10_2_9_3:
    case kExpandedLayoutLfePair:
    case kExpandedLayoutBottom3Ch:
    case kExpandedLayout7_1_5_4Ch:
    case kExpandedLayoutBottom4Ch:
    case kExpandedLayoutTop1Ch:
    case kExpandedLayoutTop5Ch:
      // Simple and Base profiles never supported expanded layouts.
      // Base-Enhanced does not support the 10.2.9.3-based or
      // 7.1.5.4-based layouts.
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      break;
    case kExpandedLayoutReserved20:
    case kExpandedLayoutReserved255:
    default:
      // Other layouts are reserved and not supported by any profile.
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseAdvancedProfile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced1Profile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced2Profile);
  }
  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context,
        "has expanded_loudspeaker_layout= ", *expanded_loudspeaker_layout,
        ". But the requested profiles do support not support this type."));
  }
  return absl::OkStatus();
}

absl::Status FilterChannelBasedConfig(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (!std::holds_alternative<ScalableChannelLayoutConfig>(
          audio_element_obu.config_)) {
    return ClearAndReturnError(
        absl::StrCat(
            debugging_context,
            "signals that it is a channel-based audio element, but it does not "
            "hold an `ScalableChannelLayoutConfig`."),
        profile_versions);
  }
  const auto& scalable_channel_layout_config =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_);
  if (scalable_channel_layout_config.channel_audio_layer_configs.empty()) {
    return ClearAndReturnError(
        absl::StrCat(debugging_context, ". Expected at least one layer."),
        profile_versions);
  }

  const auto& first_channel_audio_layer_config =
      scalable_channel_layout_config.channel_audio_layer_configs[0];

  switch (first_channel_audio_layer_config.loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kLayoutMono:
    case kLayoutStereo:
    case kLayout5_1_ch:
    case kLayout5_1_2_ch:
    case kLayout5_1_4_ch:
    case kLayout7_1_ch:
    case kLayout7_1_2_ch:
    case kLayout7_1_4_ch:
    case kLayout3_1_2_ch:
    case kLayoutBinaural:
      break;
    case kLayoutReserved10:
    case kLayoutReserved11:
    case kLayoutReserved12:
    case kLayoutReserved13:
    case kLayoutReserved14:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      // TODO(b/461488730): Ensure these agree with v2.0.0 limits.
      profile_versions.erase(ProfileVersion::kIamfBaseAdvancedProfile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced1Profile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced2Profile);
      break;
    case kLayoutExpanded:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      RETURN_IF_NOT_OK(FilterExpandedLoudspeakerLayout(
          debugging_context,
          first_channel_audio_layer_config.expanded_loudspeaker_layout,
          profile_versions));
      break;
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has the first loudspeaker_layout= ",
        first_channel_audio_layer_config.loudspeaker_layout,
        ". But the requested profiles do support not support this type."));
  }

  return absl::OkStatus();
}

absl::Status FilterAmbisonicsConfig(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (!std::holds_alternative<AmbisonicsConfig>(audio_element_obu.config_)) {
    return ClearAndReturnError(
        absl::StrCat(
            debugging_context,
            "signals that it is a scene-based audio element, but it does not "
            "hold an `AmbisonicsConfig`."),
        profile_versions);
  }

  auto ambisonics_mode =
      std::get<AmbisonicsConfig>(audio_element_obu.config_).GetAmbisonicsMode();
  switch (ambisonics_mode) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case AmbisonicsConfig::kAmbisonicsModeMono:
    case AmbisonicsConfig::kAmbisonicsModeProjection:
      break;
    default:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseAdvancedProfile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced1Profile);
      profile_versions.erase(ProfileVersion::kIamfAdvanced2Profile);
      break;
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has ambisonics_mode= ", ambisonics_mode,
        ". But the requested profiles do support not support this mode."));
  }
  return absl::OkStatus();
}

absl::Status FilterProfileForNumSubmixes(
    absl::string_view mix_presentation_id_for_debugging,
    int num_sub_mixes_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (num_sub_mixes_in_mix_presentation > 1) {
    profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
    profile_versions.erase(ProfileVersion::kIamfBaseProfile);
    profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }
  if (num_sub_mixes_in_mix_presentation > 2) {
    return ClearAndReturnError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_sub_mixes_in_mix_presentation,
                     " sub-mixes, but the requested profiles "
                     "do not support this number of sub-mixes."),
        profile_versions);
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_sub_mixes_in_mix_presentation,
                     " sub-mixes, but the requested profiles "
                     "do not support this number of sub-mixes."));
  }
  return absl::OkStatus();
}

absl::Status FilterProfileForHeadphonesRenderingMode(
    absl::string_view mix_presentation_id_for_debugging,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
    for (const auto& sub_mix_audio_element : sub_mix.audio_elements) {
      switch (
          sub_mix_audio_element.rendering_config.headphones_rendering_mode) {
        using enum RenderingConfig::HeadphonesRenderingMode;
        case kHeadphonesRenderingModeBinauralHeadLocked:
          profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
          break;
        case kHeadphonesRenderingModeReserved3:
          // No profile supports this mode.
          profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseAdvancedProfile);
          profile_versions.erase(ProfileVersion::kIamfAdvanced1Profile);
          profile_versions.erase(ProfileVersion::kIamfAdvanced2Profile);
          break;
        default:
          break;
      }

      if (profile_versions.empty()) {
        return absl::InvalidArgumentError(absl::StrCat(
            mix_presentation_id_for_debugging,
            " has an audio element with headphones rendering mode= ",
            sub_mix_audio_element.rendering_config.headphones_rendering_mode,
            " sub-mixes, but the requested profiles do support not this "
            "mode."));
      }
    }
  }

  return absl::OkStatus();
}

// Validates several conditions as described of the IAMF spec.
//
// Various rules are described in section 4 of the IAMF Spec:
//
// Condition A: Every Audio Substreams used in the first sub-mix of all Mix
//              Presentation OBUs SHALL be coded using the same Codec Config
//              OBU.
// Redundant with A: If num_sub_mixes = 1 in all Mix Presentation OBUs, there
//                   SHALL be only one unique Codec Config OBU.
//
// Condition B: Each profile has a maximum number of Codec Config OBUs per
//              Mix Presentation.
//
// Condition C: If there are two unique Codec Config OBUs, then at least one
//              of the two codec_ids SHALL be ipcm.
//
// Condition D: The frame sizes and the output sample rates identified
//              (implicitly or explicitly) by the two Codec Config OBUs SHALL
//              be the same.
absl::Status FilterProfilesForCodecConfigRules(
    absl::string_view mix_presentation_id_for_debugging,
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  struct CodecConfigInfo {
    uint32_t num_samples_per_frame;
    uint32_t output_sample_rate;
  };
  // Gather information from each Codec Config OBU.
  absl::flat_hash_map<DecodedUleb128, CodecConfigInfo> codec_config_id_to_info;
  bool found_lpcm = false;
  for (size_t i = 0; i < mix_presentation_obu.sub_mixes_.size(); ++i) {
    const auto& sub_mix = mix_presentation_obu.sub_mixes_[i];
    for (const auto& audio_element : sub_mix.audio_elements) {
      auto it = audio_elements.find(audio_element.audio_element_id);
      if (it == audio_elements.end() || it->second.codec_config == nullptr) {
        return ClearAndReturnError(
            absl::StrCat("Failed to find Codec Config for Audio Element: ",
                         audio_element.audio_element_id),
            profile_versions);
      }
      if (it->second.codec_config->GetCodecConfig().codec_id ==
          CodecConfig::CodecId::kCodecIdLpcm) {
        found_lpcm = true;
      }
      codec_config_id_to_info.emplace(
          it->second.codec_config->GetCodecConfigId(),
          CodecConfigInfo{
              .num_samples_per_frame =
                  it->second.codec_config->GetNumSamplesPerFrame(),
              .output_sample_rate =
                  it->second.codec_config->GetOutputSampleRate(),
          });
    }

    if (i == 0 && codec_config_id_to_info.size() > 1) {
      // Condition A was violated. The first sub-mix has a special rule that it
      // must only use one codec config.
      return ClearAndReturnError(
          absl::StrCat(
              mix_presentation_id_for_debugging, " references ",
              codec_config_id_to_info.size(),
              " Codec Config OBUs in the first sub-mix, but no profile "
              "supports this."),
          profile_versions);
    }
  }

  // Validate the various combinations and conditions and filter out those
  // profiles.
  constexpr std::array<std::pair<ProfileVersion, int>, 6>
      kProfileVersionAndMaxCodecConfigs = {
          {{ProfileVersion::kIamfSimpleProfile, 1},
           {ProfileVersion::kIamfBaseProfile, 1},
           {ProfileVersion::kIamfBaseEnhancedProfile, 1},
           {ProfileVersion::kIamfBaseAdvancedProfile, 2},
           {ProfileVersion::kIamfAdvanced1Profile, 2},
           {ProfileVersion::kIamfAdvanced2Profile, 2}}};
  for (const auto& [profile_version, max_codec_configs] :
       kProfileVersionAndMaxCodecConfigs) {
    if (codec_config_id_to_info.size() >
        static_cast<size_t>(max_codec_configs)) {
      // Condition B was violated. We found multiple unique codec configs, which
      // are forbidden under the older v1.1.0 profiles.
      profile_versions.erase(profile_version);
    }
  }

  if (codec_config_id_to_info.size() > 1 && !found_lpcm) {
    if (!found_lpcm) {
      // Condition C was violated. We found multiple codec configs, but none
      // were LPCM.
      return ClearAndReturnError(
          absl::StrCat(
              mix_presentation_id_for_debugging,
              " has multiple unique codec configs, but no lpcm codec config."),
          profile_versions);
    }
  }

  if (codec_config_id_to_info.empty()) {
    return ClearAndReturnError(absl::StrCat(mix_presentation_id_for_debugging,
                                            " has no codec configs."),
                               profile_versions);
  }
  const CodecConfigInfo& common_codec_config_info =
      codec_config_id_to_info.begin()->second;
  for (const auto& [codec_config_id, codec_config_info] :
       codec_config_id_to_info) {
    if (codec_config_info.num_samples_per_frame !=
            common_codec_config_info.num_samples_per_frame ||
        common_codec_config_info.output_sample_rate !=
            codec_config_info.output_sample_rate) {
      // Condition D was violated. We found multiple unique codec configs with
      // different frame sizes or output sample rates.
      return ClearAndReturnError(
          absl::StrCat(
              mix_presentation_id_for_debugging,
              " has codec config with different properties, "
              "num_samples_per_frame= ",
              common_codec_config_info.num_samples_per_frame,
              " sample_rate= ", common_codec_config_info.output_sample_rate),
          profile_versions);
    }
  }

  return absl::OkStatus();
}

int GetNumberOfChannels(const AudioElementWithData& audio_element) {
  int num_channels = 0;
  for (const auto& [substream_id, labels] :
       audio_element.substream_id_to_labels) {
    num_channels += labels.size();
  }
  return num_channels;
}

absl::Status FilterAudioElementsAndGetNumberOfAudioElementsAndChannels(
    absl::string_view mix_presentation_id_for_debugging,
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions,
    int& num_audio_elements_in_mix_presentation,
    int& num_channels_in_mix_presentation) {
  num_audio_elements_in_mix_presentation = 0;
  num_channels_in_mix_presentation = 0;
  for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
    num_audio_elements_in_mix_presentation += sub_mix.audio_elements.size();
    for (const auto& sub_mix_audio_element : sub_mix.audio_elements) {
      auto iter = audio_elements.find(sub_mix_audio_element.audio_element_id);
      if (iter == audio_elements.end()) {
        return ClearAndReturnError(
            absl::StrCat(mix_presentation_id_for_debugging,
                         " has Audio Element ID= ",
                         sub_mix_audio_element.audio_element_id,
                         " , but there is no Audio Element with that ID."),
            profile_versions);
      }
      RETURN_IF_NOT_OK(ProfileFilter::FilterProfilesForAudioElement(
          mix_presentation_id_for_debugging, iter->second.obu,
          profile_versions));

      num_channels_in_mix_presentation += GetNumberOfChannels(iter->second);
    }
  }
  return absl::OkStatus();
}

absl::Status FilterProfilesForNumAudioElements(
    absl::string_view mix_presentation_id_for_debugging,
    int num_audio_elements_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  // TODO(b/461488730): Ensure these agree with final v2.0.0 limits.
  using enum ProfileVersion;
  constexpr auto kProfileVersionAndMaxAudioElements =
      std::to_array<std::pair<ProfileVersion, int>>({
          {kIamfSimpleProfile, 1},
          {kIamfBaseProfile, 2},
          {kIamfBaseEnhancedProfile, 28},
          {kIamfBaseAdvancedProfile, 18},
          {kIamfAdvanced1Profile, 18},
          {kIamfAdvanced2Profile, 28},
      });
  for (const auto& [profile_version, max_audio_elements] :
       kProfileVersionAndMaxAudioElements) {
    if (num_audio_elements_in_mix_presentation > max_audio_elements) {
      profile_versions.erase(profile_version);
    }
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_audio_elements_in_mix_presentation,
                     " audio elements, but no "
                     "profile supports this number of audio elements."));
  }

  return absl::OkStatus();
}

absl::Status FilterProfilesForNumChannels(
    absl::string_view mix_presentation_id_for_debugging,
    int num_channels_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  // TODO(b/461488730): Ensure these agree with final v2.0.0 limits.
  using enum ProfileVersion;
  constexpr auto kProfileVersionAndMaxChannels =
      std::to_array<std::pair<ProfileVersion, int>>({
          {kIamfSimpleProfile, 16},
          {kIamfBaseProfile, 18},
          {kIamfBaseEnhancedProfile, 28},
          {kIamfBaseAdvancedProfile, 18},
          {kIamfAdvanced1Profile, 18},
          {kIamfAdvanced2Profile, 28},
      });
  for (const auto& [profile_version, max_channels] :
       kProfileVersionAndMaxChannels) {
    if (num_channels_in_mix_presentation > max_channels) {
      profile_versions.erase(profile_version);
    }
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_channels_in_mix_presentation,
                     " channels, but no "
                     "profile supports this number of channels."));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status ProfileFilter::FilterProfilesForAudioElement(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  const std::string context_and_audio_element_id_for_debugging = absl::StrCat(
      debugging_context,
      " Audio element ID= ", audio_element_obu.GetAudioElementId(), " ");

  RETURN_IF_NOT_OK(FilterAudioElementType(
      context_and_audio_element_id_for_debugging,
      audio_element_obu.GetAudioElementType(), profile_versions));
  // Filter any type-specific properties.
  switch (audio_element_obu.GetAudioElementType()) {
    using enum AudioElementObu::AudioElementType;
    case AudioElementObu::kAudioElementChannelBased:
      RETURN_IF_NOT_OK(
          FilterChannelBasedConfig(context_and_audio_element_id_for_debugging,
                                   audio_element_obu, profile_versions));
      break;
    case AudioElementObu::kAudioElementSceneBased:
      RETURN_IF_NOT_OK(
          FilterAmbisonicsConfig(context_and_audio_element_id_for_debugging,
                                 audio_element_obu, profile_versions));
      break;
    default:
      break;
  }

  return absl::OkStatus();
}

absl::Status ProfileFilter::FilterProfilesForMixPresentation(
    const DescriptorObus::AudioElementsById& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  const std::string mix_presentation_id_for_debugging =
      absl::StrCat("Mix presentation with ID= ",
                   mix_presentation_obu.GetMixPresentationId());
  MAYBE_RETURN_IF_NOT_OK(FilterProfileForNumSubmixes(
      mix_presentation_id_for_debugging, mix_presentation_obu.GetNumSubMixes(),
      profile_versions));

  MAYBE_RETURN_IF_NOT_OK(FilterProfileForHeadphonesRenderingMode(
      mix_presentation_id_for_debugging, mix_presentation_obu,
      profile_versions));

  RETURN_IF_NOT_OK(FilterProfilesForCodecConfigRules(
      mix_presentation_id_for_debugging, audio_elements, mix_presentation_obu,
      profile_versions));

  int num_audio_elements_in_mix_presentation;
  int num_channels_in_mix_presentation;
  MAYBE_RETURN_IF_NOT_OK(
      FilterAudioElementsAndGetNumberOfAudioElementsAndChannels(
          mix_presentation_id_for_debugging, audio_elements,
          mix_presentation_obu, profile_versions,
          num_audio_elements_in_mix_presentation,
          num_channels_in_mix_presentation));

  MAYBE_RETURN_IF_NOT_OK(FilterProfilesForNumAudioElements(
      mix_presentation_id_for_debugging, num_audio_elements_in_mix_presentation,
      profile_versions));
  MAYBE_RETURN_IF_NOT_OK(FilterProfilesForNumChannels(
      mix_presentation_id_for_debugging, num_channels_in_mix_presentation,
      profile_versions));

  return absl::OkStatus();
}

}  // namespace iamf_tools
