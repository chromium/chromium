/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/renderer/default_layout_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status FlushUntilNonEmptyOrTimeout(
    AudioElementRendererBase& audio_element_renderer,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  static const int kMaxNumTries = 500;
  for (int i = 0; i < kMaxNumTries; i++) {
    audio_element_renderer.Flush(rendered_samples);
    if (std::none_of(rendered_samples.begin(), rendered_samples.end(),
                     [](const auto& channel) { return channel.empty(); })) {
      // Usually samples will be ready right away. So avoid sleeping.
      return absl::OkStatus();
    }
    absl::SleepFor(absl::Milliseconds(10));
  }
  return absl::DeadlineExceededError("Timed out waiting for samples.");
}

absl::Status RenderLabeledFrameToLayout(
    const LabeledFrame& labeled_frame, const uint32_t num_samples_per_frame,
    AudioElementRendererBase& renderer,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  const auto num_time_ticks = renderer.RenderLabeledFrame(labeled_frame);

  for (auto& rendered_samples_for_channel : rendered_samples) {
    rendered_samples_for_channel.clear();
  }
  if (!num_time_ticks.ok()) {
    return num_time_ticks.status();
  } else if (*num_time_ticks > num_samples_per_frame) {
    return absl::InvalidArgumentError("Too many samples in this frame");
  } else if (*num_time_ticks == 0) {
    // This was an empty frame. Flush to get the number of channels right even
    // when there is no actual sample.
    renderer.Flush(rendered_samples);
    return absl::OkStatus();
  }

  return FlushUntilNonEmptyOrTimeout(renderer, rendered_samples);
}

// Fills in the output `mix_gains` with the gain in Q7.8 format to apply at each
// tick.
absl::Status GetParameterBlockLinearMixGainsPerTick(
    uint32_t common_sample_rate,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain,
    std::vector<float>& linear_mix_gain_per_tick) {
  if (mix_gain.GetParameterRate() != common_sample_rate) {
    // TODO(b/283281856): Support resampling parameter blocks.
    return absl::UnimplementedError(absl::StrCat(
        "Parameter blocks that require resampling are not supported yet: (",
        mix_gain.GetParameterRate(), " != ", common_sample_rate, ")"));
  }

  // Initialize to the default gain value.
  std::fill(
      linear_mix_gain_per_tick.begin(), linear_mix_gain_per_tick.end(),
      std::pow(10.0f, mix_gain.default_mix_gain_.GetFloatingPoint() / 20.0f));
  auto parameter_block_iter =
      id_to_parameter_block.find(mix_gain.GetParameterId());
  if (parameter_block_iter == id_to_parameter_block.end()) {
    // Default mix gain will be used for this frame. Logic elsewhere validates
    // the rest of the audio frames have consistent coverage.
    return absl::OkStatus();
  }
  const auto& parameter_block = *parameter_block_iter->second;
  RETURN_IF_NOT_OK(
      parameter_block.obu->GetLinearMixGains(linear_mix_gain_per_tick));
  return absl::OkStatus();
}

absl::Status GetAndApplyMixGain(
    uint32_t common_sample_rate,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain, int32_t num_channels,
    std::vector<float>& linear_mix_gain_per_tick,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual("rendered_samples",
                                              rendered_samples, num_channels));

  const auto num_ticks =
      rendered_samples.empty() ? 0 : rendered_samples[0].size();

  // Get the mix gain on a per tick basis from the parameter block.
  linear_mix_gain_per_tick.resize(num_ticks);
  RETURN_IF_NOT_OK(GetParameterBlockLinearMixGainsPerTick(
      common_sample_rate, id_to_parameter_block, mix_gain,
      linear_mix_gain_per_tick));

  if (!linear_mix_gain_per_tick.empty()) {
    ABSL_LOG_FIRST_N(INFO, 6) << "First tick in this frame has gain: "
                              << linear_mix_gain_per_tick.front();
  }

  for (auto& rendered_samples_for_channel : rendered_samples) {
    for (size_t tick = 0; tick < num_ticks; tick++) {
      // Apply the mix gain per tick to all channels.
      rendered_samples_for_channel[tick] *= linear_mix_gain_per_tick[tick];
    }
  }

  return absl::OkStatus();
}

absl::Status MixAudioElements(
    std::vector<std::vector<std::vector<InternalSampleType>>>&
        rendered_audio_elements,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  const auto num_audio_elements = rendered_audio_elements.size();
  const auto num_channels = rendered_audio_elements.empty()
                                ? 0
                                : rendered_audio_elements.front().size();
  const auto num_ticks =
      num_channels == 0 ? 0 : rendered_audio_elements.front().front().size();
  ABSL_CHECK_EQ(rendered_samples.size(), num_channels);
  for (auto& rendered_samples_for_channel : rendered_samples) {
    // To avoid re-allocations, `rendered_samples_for_channel` should already
    // be allocated with the maximum number of samples that it will hold, which
    // is the number of samples per frame. Rendering a partial (therefore
    // smaller) frame is allowed.
    ABSL_CHECK_GE(rendered_samples_for_channel.capacity(), num_ticks);
    rendered_samples_for_channel.assign(num_ticks, 0.0);
  }

  // Expect all frames have the same number of channels and all channels
  // have the same number of ticks.
  for (const auto& rendered_audio_element : rendered_audio_elements) {
    RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
        "rendered_audio_element", rendered_audio_element, num_channels));
    for (const auto& samples_for_channel : rendered_audio_element) {
      RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
          "samples_for_channel", samples_for_channel, num_ticks));
    }
  }

  for (size_t a = 0; a < num_audio_elements; a++) {
    const auto& rendered_samples_for_audio_element = rendered_audio_elements[a];
    for (size_t c = 0; c < num_channels; c++) {
      auto& rendered_samples_for_channel = rendered_samples[c];
      const auto& rendered_samples_for_audio_element_for_channel =
          rendered_samples_for_audio_element[c];
      for (size_t t = 0; t < num_ticks; t++) {
        // Sum all audio elements for this (channel, tick).
        rendered_samples_for_channel[t] +=
            rendered_samples_for_audio_element_for_channel[t];
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<DefaultLayoutRenderer> DefaultLayoutRenderer::Create(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain, const Layout& layout,
    int32_t num_channels, uint32_t common_sample_rate,
    uint32_t common_num_samples_per_frame,
    const RendererFactoryBase& renderer_factory) {
  const size_t num_audio_elements = audio_elements_in_sub_mix.size();
  ABSL_CHECK_OK(ValidateContainerSizeEqual(
      "sub_mix_audio_elements", sub_mix_audio_elements, num_audio_elements));

  std::vector<std::unique_ptr<AudioElementRendererBase>> renderers(
      num_audio_elements);
  for (size_t i = 0; i < num_audio_elements; i++) {
    const auto& audio_element = *audio_elements_in_sub_mix[i];
    renderers[i] = renderer_factory.CreateRendererForLayout(
        audio_element.obu.audio_substream_ids_,
        audio_element.substream_id_to_labels,
        audio_element.obu.GetAudioElementType(), audio_element.obu.config_,
        sub_mix_audio_elements[i].rendering_config, layout,
        static_cast<size_t>(common_num_samples_per_frame),
        static_cast<size_t>(common_sample_rate));
    if (renderers[i] == nullptr) {
      return nullptr;
    }
  }

  std::vector<DecodedUleb128> audio_element_ids;
  std::vector<MixGainParamDefinition> element_mix_gains;
  audio_element_ids.reserve(num_audio_elements);
  element_mix_gains.reserve(num_audio_elements);
  for (const auto& sub_mix_audio_element : sub_mix_audio_elements) {
    audio_element_ids.emplace_back(sub_mix_audio_element.audio_element_id);
    element_mix_gains.emplace_back(sub_mix_audio_element.element_mix_gain);
  }
  return absl::WrapUnique(new DefaultLayoutRenderer(
      audio_element_ids, element_mix_gains, output_mix_gain,
      std::move(renderers), num_channels, common_sample_rate,
      common_num_samples_per_frame));
}

absl::Status DefaultLayoutRenderer::Render(
    const IdLabeledFrameMap& id_to_labeled_frame,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    std::vector<std::vector<InternalSampleType>>& rendered_samples,
    std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples) {
  // Each audio element rendered individually with `element_mix_gain` applied.
  const size_t num_audio_elements = audio_element_ids_.size();
  for (size_t i = 0; i < num_audio_elements; i++) {
    const auto& audio_element_id = audio_element_ids_[i];
    const auto& element_mix_gain = element_mix_gains_[i];
    if (id_to_labeled_frame.find(audio_element_id) !=
        id_to_labeled_frame.end()) {
      const auto& labeled_frame = id_to_labeled_frame.at(audio_element_id);
      // Render the frame to the specified `loudness_layout` and apply element
      // mix gain.
      RETURN_IF_NOT_OK(RenderLabeledFrameToLayout(
          labeled_frame, common_num_samples_per_frame_, *renderers_[i],
          rendered_audio_elements_[i]));
    }

    RETURN_IF_NOT_OK(GetAndApplyMixGain(
        common_sample_rate_, id_to_parameter_block, element_mix_gain,
        num_channels_, linear_mix_gain_per_tick_, rendered_audio_elements_[i]));
  }

  // Mix the audio elements.
  RETURN_IF_NOT_OK(
      MixAudioElements(rendered_audio_elements_, rendered_samples));

  ABSL_LOG_FIRST_N(INFO, 1) << "    Applying output_mix_gain.default_mix_gain= "
                            << output_mix_gain_.default_mix_gain_;

  RETURN_IF_NOT_OK(GetAndApplyMixGain(
      common_sample_rate_, id_to_parameter_block, output_mix_gain_,
      num_channels_, linear_mix_gain_per_tick_, rendered_samples));

  // TODO(b/542068876): Clip audio sample values to [-1, 1] here.

  valid_rendered_samples.resize(rendered_samples.size());
  for (size_t c = 0; c < rendered_samples.size(); ++c) {
    valid_rendered_samples[c] = absl::MakeConstSpan(rendered_samples[c]);
  }

  return absl::OkStatus();
}

DefaultLayoutRenderer::DefaultLayoutRenderer(
    const std::vector<DecodedUleb128>& audio_element_ids,
    const std::vector<MixGainParamDefinition>& element_mix_gains,
    const MixGainParamDefinition& output_mix_gain,
    std::vector<std::unique_ptr<AudioElementRendererBase>> renderers,
    int32_t num_channels, uint32_t common_sample_rate,
    uint32_t common_num_samples_per_frame)
    : LayoutRendererBase(audio_element_ids, element_mix_gains, output_mix_gain,
                         num_channels, common_sample_rate,
                         common_num_samples_per_frame),
      renderers_(std::move(renderers)),
      rendered_audio_elements_(audio_element_ids.size()) {}

}  // namespace iamf_tools
