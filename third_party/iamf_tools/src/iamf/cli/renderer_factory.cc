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
#include "iamf/cli/renderer_factory.h"

#include <cstddef>
#include <memory>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/audio_element_renderer_channel_to_channel.h"
#include "iamf/cli/renderer/audio_element_renderer_passthrough.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

// These directives are not part of an official API and are likely to change or
// be removed. Please do not depend on them.
#ifndef IAMF_TOOLS_DISABLE_BINAURAL_RENDERING
#include "iamf/cli/renderer/audio_element_renderer_binaural.h"
#endif

namespace iamf_tools {

namespace {

bool IsAudioElementRenderedBinaural(
    RenderingConfig::HeadphonesRenderingMode headphones_rendering_mode,
    Layout::LayoutType layout_type) {
  {
    // TODO(b/282877209): Check for kHeadphonesRenderingModeBinauralHeadLocked`.
    //                    Ensure it is plumbed to the renderer.
    return headphones_rendering_mode ==
               RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked &&
           layout_type == Layout::kLayoutTypeBinaural;
  }
}

std::unique_ptr<AudioElementRendererBase> MaybeCreateAmbisonicsRenderer(
    bool use_binaural, const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const AudioElementObu::AudioElementConfig& config,
    const Layout& playback_layout, size_t num_samples_per_frame,
    size_t sample_rate, const TrimmingSettings trimming_settings) {
  const auto* ambisonics_config = std::get_if<AmbisonicsConfig>(&config);
  if (ambisonics_config == nullptr) {
    ABSL_LOG(ERROR)
        << "Ambisonics config is inconsistent with audio element type.";
    return nullptr;
  }

  if (use_binaural) {
#ifndef IAMF_TOOLS_DISABLE_BINAURAL_RENDERING
    return AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
        *ambisonics_config, audio_substream_ids, substream_id_to_labels,
        num_samples_per_frame, sample_rate, trimming_settings);
#else
    ABSL_LOG(WARNING)
        << "Skipping creating an Ambisonics to binaural-based "
           "renderer. Binaural rendering is not yet supported for "
           "ambisonics.";
    return nullptr;
#endif
  }
  return AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
      *ambisonics_config, audio_substream_ids, substream_id_to_labels,
      playback_layout, num_samples_per_frame, trimming_settings);
}

std::unique_ptr<AudioElementRendererBase> MaybeCreateChannelRenderer(
    bool use_binaural, const AudioElementObu::AudioElementConfig& config,
    const Layout& playback_layout, size_t num_samples_per_frame,
    size_t sample_rate, const TrimmingSettings trimming_settings) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&config);
  if (channel_config == nullptr) {
    ABSL_LOG(ERROR)
        << "Channel config is inconsistent with audio element type.";
    return nullptr;
  }

  // Lazily try to make a pass-through renderer.
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          *channel_config, playback_layout, num_samples_per_frame,
          trimming_settings);
  if (pass_through_renderer != nullptr) {
    return pass_through_renderer;
  }

  if (use_binaural) {
#ifndef IAMF_TOOLS_DISABLE_BINAURAL_RENDERING
    return AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
        *channel_config, num_samples_per_frame, sample_rate, trimming_settings);
#else
    ABSL_LOG(WARNING)
        << "Skipping creating a channel to binaural-based renderer.";
    return nullptr;
#endif
  }
  return AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(*channel_config, playback_layout,
                                            num_samples_per_frame,
                                            trimming_settings);
}

}  // namespace

RendererFactoryBase::~RendererFactoryBase() {}

std::unique_ptr<AudioElementRendererBase>
RendererFactory::CreateRendererForLayout(
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    AudioElementObu::AudioElementType audio_element_type,
    const AudioElementObu::AudioElementConfig& audio_element_config,
    const RenderingConfig& rendering_config, const Layout& loudness_layout,
    size_t num_samples_per_frame, size_t sample_rate) const {
  Layout playback_layout = loudness_layout;
  const bool use_binaural = IsAudioElementRenderedBinaural(
      rendering_config.headphones_rendering_mode, playback_layout.layout_type);

  // If `!use_binaural` but the playback layout is binaural, this is because
  // `headphones_rendering_mode` is set to stereo. In this case, fake the
  // playback layout to be stereo.
  if (!use_binaural &&
      playback_layout.layout_type == Layout::kLayoutTypeBinaural) {
    playback_layout = {
        .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
        .specific_layout = LoudspeakersSsConventionLayout{
            .sound_system =
                LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};
  }

  std::unique_ptr<AudioElementRendererBase> renderer;
  switch (audio_element_type) {
    case AudioElementObu::kAudioElementSceneBased:
      renderer = MaybeCreateAmbisonicsRenderer(
          use_binaural, audio_substream_ids, substream_id_to_labels,
          audio_element_config, playback_layout, num_samples_per_frame,
          sample_rate, trimming_settings_);
      break;
    case AudioElementObu::kAudioElementChannelBased:
      renderer = MaybeCreateChannelRenderer(
          use_binaural, audio_element_config, playback_layout,
          num_samples_per_frame, sample_rate, trimming_settings_);
      break;
    case AudioElementObu::kAudioElementObjectBased:
      ABSL_LOG(WARNING) << "Object-based audio elements are not supported.";
      break;
    case AudioElementObu::kAudioElementBeginReserved:
    case AudioElementObu::kAudioElementEndReserved:
      ABSL_LOG(WARNING) << "Unsupported audio_element_type_= "
                        << audio_element_type;
      break;
  }
  return renderer;
}

}  // namespace iamf_tools
