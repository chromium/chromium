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

#include "iamf/cli/downmixer_manager.h"

#include <cmath>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {

std::unique_ptr<DownmixerManager> DownmixerManager::Make(
    absl::flat_hash_map<DecodedUleb128, DownmixingConfig> id_to_config_map) {
  return absl::WrapUnique(new DownmixerManager(std::move(id_to_config_map)));
}

std::unique_ptr<DownmixerManager> DownmixerManager::MakeForPassthrough(
    const DescriptorObus::AudioElementsById& audio_elements) {
  absl::flat_hash_map<DecodedUleb128, DownmixingConfig>
      audio_element_id_to_downmixing_config;

  // Configure a minimal config (without down-mixers) to trigger passthrough
  // mode.
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    audio_element_id_to_downmixing_config.emplace(
        audio_element_id,
        DownmixingConfig{
            .down_mixers = {},
            .substream_id_to_labels =
                audio_element_with_data.substream_id_to_labels,
            .label_to_output_gain =
                audio_element_with_data.label_to_output_gain,
        });
  }

  return Make(std::move(audio_element_id_to_downmixing_config));
}

absl::Status DownmixerManager::DownMixSamplesToSubstreams(
    DecodedUleb128 audio_element_id, const DownMixingParams& down_mixing_params,
    LabelSamplesMap& input_label_to_samples,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) {
  auto iter = audio_element_id_to_downmixing_config_.find(audio_element_id);
  if (iter == audio_element_id_to_downmixing_config_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Audio element ID not found: ", audio_element_id));
  }
  DownmixingConfig& downmixer_metadata = iter->second;

  // First perform all the down mixing.
  for (auto& down_mixer : downmixer_metadata.down_mixers) {
    RETURN_IF_NOT_OK(down_mixer(down_mixing_params, input_label_to_samples));
  }

  for (const auto& [substream_id, output_channel_labels] :
       downmixer_metadata.substream_id_to_labels) {
    // Find the `SubstreamData` with this `substream_id`.
    auto substream_data_iter =
        substream_id_to_substream_data.find(substream_id);
    if (substream_data_iter == substream_id_to_substream_data.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find substream data for substream ID= ", substream_id));
    }
    auto& substream_data = substream_data_iter->second;

    int channel_index = 0;
    for (const auto& output_channel_label : output_channel_labels) {
      // Compute and store the linear output gains for this channel.
      const auto gain_iter =
          downmixer_metadata.label_to_output_gain.find(output_channel_label);
      const double output_gain_linear =
          (gain_iter == downmixer_metadata.label_to_output_gain.end())
              ? 1.0
              : std::pow(10.0, gain_iter->second / 20.0);
      auto samples_iter = input_label_to_samples.find(output_channel_label);
      if (samples_iter == input_label_to_samples.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Samples do not exist for channel: ", output_channel_label));
      }
      const auto& input_samples = samples_iter->second;

      // Add all down mixed samples to both substream frames.
      for (const auto input_sample : input_samples) {
        substream_data.frames_in_obu.PushSample(channel_index, input_sample);

        // Apply output gains to the samples going to the encoder and also
        // convert the samples to 32-bit integers.
        int32_t attenuated_sample_int32 = 0;
        RETURN_IF_NOT_OK(NormalizedFloatingPointToInt32(
            input_sample / output_gain_linear, attenuated_sample_int32));
        substream_data.frames_to_encode.PushSample(channel_index,
                                                   attenuated_sample_int32);
      }
      channel_index++;
    }
  }

  return absl::OkStatus();
}

bool DownmixerManager::HasDownMixers(DecodedUleb128 audio_element_id) const {
  auto iter = audio_element_id_to_downmixing_config_.find(audio_element_id);
  if (iter == audio_element_id_to_downmixing_config_.end()) {
    return false;
  }
  return !iter->second.down_mixers.empty();
}

}  // namespace iamf_tools
