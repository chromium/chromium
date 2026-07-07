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
#include "iamf/cli/cli_util.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status InsertParamDefinitionAndCheckEquivalence(
    const ParamDefinitionVariant& param_definition_variant_to_insert,
    absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants) {
  const auto parameter_id = std::visit(
      [](const auto& param_definition) {
        return param_definition.GetParameterId();
      },
      param_definition_variant_to_insert);
  const auto [existing_param_definition_iter, inserted] =
      param_definition_variants.insert(
          {parameter_id, param_definition_variant_to_insert});

  // Use double dispatch to check equivalence. Note this automatically returns
  // false when the two variants do not hold the same type of objects.
  const auto equivalent_to_param_definition_variant_to_insert =
      [&param_definition_variant_to_insert](const auto& rhs) {
        return std::visit([&rhs](const auto& lhs) { return (lhs == rhs); },
                          param_definition_variant_to_insert);
      };

  if (!inserted && !std::visit(equivalent_to_param_definition_variant_to_insert,
                               existing_param_definition_iter->second)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inequivalent `param_definition` for id = ", parameter_id));
  }

  return absl::OkStatus();
}

absl::Status FillReconGainAuxiliaryData(
    const AudioElementWithData& audio_element,
    std::vector<ReconGainParamDefinition::ReconGainAuxiliaryData>& aux_data) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&audio_element.obu.config_);
  RETURN_IF_NOT_OK(ValidateNotNull(
      channel_config,
      "Channel config expected when there is a gain param definition."));
  aux_data.reserve(channel_config->GetNumLayers());
  for (int l = 0; l < channel_config->GetNumLayers(); l++) {
    aux_data.emplace_back(ReconGainParamDefinition::ReconGainAuxiliaryData{
        .recon_gain_is_present_flag =
            channel_config->channel_audio_layer_configs[l]
                .recon_gain_is_present_flag,
        .channel_numbers_for_layer =
            audio_element.channel_numbers_for_layers[l]});
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status CollectAndValidateParamDefinitions(
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants) {
  param_definition_variants.clear();

  // Collect all `param_definition`s in Audio Element and Mix Presentation
  // OBUs.
  for (const auto& [audio_element_id_for_debugging, audio_element] :
       audio_elements) {
    for (const auto& audio_element_param :
         audio_element.obu.audio_element_params_) {
      const auto param_definition_type = audio_element_param.GetType();
      switch (param_definition_type) {
        case ParamDefinition::kParameterDefinitionDemixing:
          RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
              std::get<DemixingParamDefinition>(
                  audio_element_param.param_definition),
              param_definition_variants));
          break;
        case ParamDefinition::kParameterDefinitionReconGain: {
          // Make a copy, which will be modified.
          ReconGainParamDefinition recon_gain_param_definition =
              std::get<ReconGainParamDefinition>(
                  audio_element_param.param_definition);
          RETURN_IF_NOT_OK(FillReconGainAuxiliaryData(
              audio_element, recon_gain_param_definition.aux_data_));
          RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
              recon_gain_param_definition, param_definition_variants));
          break;
        }
        default:
          ABSL_LOG(WARNING)
              << "Ignoring parameter definition of type= "
              << param_definition_type
              << " in audio element= " << audio_element_id_for_debugging;
          continue;
      }
    }
  }

  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
      for (const auto& audio_element : sub_mix.audio_elements) {
        for (const auto& rendering_param_definition :
             audio_element.rendering_config
                 .rendering_config_param_definitions) {
          RETURN_IF_NOT_OK(std::visit(
              [&](const auto& value) -> absl::Status {
                return InsertParamDefinitionAndCheckEquivalence(
                    value, param_definition_variants);
              },
              rendering_param_definition.param_definition));
        }
        RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
            audio_element.element_mix_gain, param_definition_variants));
      }
      RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
          sub_mix.output_mix_gain, param_definition_variants));
    }
  }

  return absl::OkStatus();
}

absl::Status CompareTimestamps(InternalTimestamp expected_timestamp,
                               InternalTimestamp actual_timestamp,
                               absl::string_view prompt) {
  if (expected_timestamp != actual_timestamp) {
    return absl::InvalidArgumentError(
        absl::StrCat(prompt, "Expected timestamp != actual timestamp: (",
                     expected_timestamp, " vs ", actual_timestamp, ")"));
  }
  return absl::OkStatus();
}

absl::Status WritePcmFrameToBuffer(
    const std::vector<std::vector<int32_t>>& frame, uint8_t bit_depth,
    bool big_endian, std::vector<uint8_t>& buffer) {
  if (bit_depth % 8 != 0) [[unlikely]] {
    return absl::InvalidArgumentError(
        "This function only supports an integer number of bytes.");
  }
  const size_t num_samples = frame.size() * frame[0].size();
  buffer.resize(num_samples * (bit_depth / 8));
  if (num_samples == 0) {
    return absl::OkStatus();
  }

  // The input frame is arranged in (channel, time) axes. Interlace these in
  // the output PCM.
  size_t write_position = 0;
  for (size_t t = 0; t < frame[0].size(); ++t) {
    for (size_t c = 0; c < frame.size(); c++) {
      const uint32_t sample = static_cast<uint32_t>(frame[c][t]);
      RETURN_IF_NOT_OK(WritePcmSample(sample, bit_depth, big_endian,
                                      buffer.data(), write_position));
    }
  }

  return absl::OkStatus();
}

absl::Status GetCommonSampleRateAndBitDepth(
    const absl::flat_hash_set<uint32_t>& sample_rates,
    const absl::flat_hash_set<uint8_t>& bit_depths,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  requires_resampling = false;
  if (sample_rates.empty() || bit_depths.empty()) {
    return absl::InvalidArgumentError(
        "Expected at least one sample rate and bit depth.");
  }

  if (sample_rates.size() == 1) {
    common_sample_rate = *sample_rates.begin();
  } else {
    // No common sample rate. The spec recommends the rendering output to be
    // resampled to 48000 Hz.
    common_sample_rate = 48000;
    requires_resampling = true;
  }

  if (bit_depths.size() == 1) {
    common_bit_depth = *bit_depths.begin();
  } else {
    // No common bit-depth. The spec recommends the rendering output to be
    // resampled to 16-bits.
    common_bit_depth = 16;
  }

  return absl::OkStatus();
}

absl::Status GetCommonSamplesPerFrame(
    const DescriptorObus::CodecConfigsById& codec_config_obus,
    uint32_t& common_samples_per_frame) {
  bool first = true;
  for (const auto& [unused_id, codec_config_obu] : codec_config_obus) {
    if (first) {
      common_samples_per_frame = codec_config_obu.GetNumSamplesPerFrame();
      first = false;
      continue;
    }

    if (common_samples_per_frame != codec_config_obu.GetNumSamplesPerFrame()) {
      return absl::UnknownError(
          "The encoder does not support Codec Config OBUs with a different "
          "number of samples per frame yet.");
    }
  }

  return absl::OkStatus();
}

void LogChannelNumbers(const std::string& name,
                       const ChannelNumbers& channel_numbers) {
  ABSL_VLOG(1) << name << ": [" << channel_numbers.surround << "."
               << channel_numbers.lfe << "." << channel_numbers.height << "]";
}

}  // namespace iamf_tools
