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
#include "iamf/cli/parameters_manager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/log/absl_vlog_is_on.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

template <typename StateType>
absl::Status UpdateParameterState(
    DecodedUleb128 audio_element_id, InternalTimestamp expected_next_timestamp,
    absl::flat_hash_map<DecodedUleb128, StateType>& parameter_states,
    absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        parameter_blocks,
    absl::string_view parameter_name,
    std::optional<StateType*>& parameter_state) {
  const auto parameter_states_iter = parameter_states.find(audio_element_id);
  if (parameter_states_iter == parameter_states.end()) {
    // No parameter definition found for the audio element ID, so
    // nothing to update.
    return absl::OkStatus();
  }

  // Validate the timestamps before updating.
  parameter_state = &parameter_states_iter->second;

  // Using `.at()` here is safe because if the parameter state exists for the
  // `audio_element_id`, an entry in `parameter_blocks` with the key
  // `parameter_state->param_definition->GetParameterId()` has already been
  // created during `Initialize()`.
  auto& parameter_block = parameter_blocks.at(
      (*parameter_state)->param_definition->GetParameterId());
  if (parameter_block == nullptr) {
    // No parameter block found for this ID. Do not validate the timestamp
    // or update anything else. Setting `parameter_state` to `std::nullopt`
    // prevents the state being updated later.
    parameter_state = std::nullopt;
    return absl::OkStatus();
  }

  // Update the next timestamp for the next frame.
  (*parameter_state)->next_timestamp = parameter_block->end_timestamp;
  RETURN_IF_NOT_OK(CompareTimestamps(
      expected_next_timestamp, (*parameter_state)->next_timestamp,
      absl::StrCat("When updating states for ", parameter_name,
                   " parameters: ")));

  // Clear out the parameter block, which should not be used before a new
  // one is added via `Add*ParameterBlock()`.
  parameter_block = nullptr;

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ParametersManager> absl_nonnull>
ParametersManager::Create(
    const DescriptorObus::AudioElementsById& audio_elements) {
  // Collect all `DemixingParamDefinition`s and all `ReconGainParamDefinitions`
  // in all Audio Elements. Validate there is no more than one per Audio
  // Element.
  // Mapping from Parameter ID to demixing parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      demixing_parameter_blocks;
  // Mapping from Parameter ID to recon gain parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      recon_gain_parameter_blocks;
  // Mapping from Audio Element ID to the demixing state.
  absl::flat_hash_map<DecodedUleb128, DemixingState> demixing_states;
  // Mapping from Audio Element ID to the recon gain state.
  absl::flat_hash_map<DecodedUleb128, ReconGainState> recon_gain_states;
  for (const auto& [audio_element_id, audio_element] : audio_elements) {
    const DemixingParamDefinition* demixing_param_definition = nullptr;
    const ReconGainParamDefinition* recon_gain_param_definition = nullptr;
    for (const auto& param : audio_element.obu.audio_element_params_) {
      const auto param_definition_type = param.GetType();
      if (param_definition_type ==
          ParamDefinition::kParameterDefinitionDemixing) {
        if (demixing_param_definition != nullptr) {
          return absl::InvalidArgumentError(
              "Not allowed to have multiple demixing parameters in a "
              "single Audio Element.");
        }
        demixing_param_definition =
            std::get_if<DemixingParamDefinition>(&param.param_definition);
        ABSL_CHECK_NE(demixing_param_definition, nullptr);

        // Continue searching. Only to validate that there is at most one
        // `DemixingParamDefinition`.
      } else if (param_definition_type ==
                 ParamDefinition::kParameterDefinitionReconGain) {
        if (recon_gain_param_definition != nullptr) {
          return absl::InvalidArgumentError(
              "Not allowed to have multiple recon gain parameters in a "
              "single Audio Element.");
        }
        recon_gain_param_definition =
            std::get_if<ReconGainParamDefinition>(&param.param_definition);
        ABSL_CHECK_NE(recon_gain_param_definition, nullptr);
      }
    }

    if (demixing_param_definition != nullptr) {
      // Insert a `nullptr` for a parameter ID. If no parameter blocks have
      // this parameter ID, then it will remain null and default values will
      // be used.
      demixing_parameter_blocks.insert(
          {demixing_param_definition->GetParameterId(), nullptr});
      demixing_states[audio_element_id] = {
          .param_definition = demixing_param_definition,
          .previous_w_idx = 0,
          .next_timestamp = 0,
          .update_rule = DemixingInfoParameterData::kFirstFrame,
      };
    }
    if (recon_gain_param_definition != nullptr) {
      // Insert a `nullptr` for a parameter ID. If no parameter blocks have
      // this parameter ID, then it will remain null and default values will
      // be used.
      recon_gain_parameter_blocks.insert(
          {recon_gain_param_definition->GetParameterId(), nullptr});
      recon_gain_states[audio_element_id] = {
          .param_definition = recon_gain_param_definition,
          .next_timestamp = 0,
      };
    }
  }

  return absl::WrapUnique(new ParametersManager(
      demixing_parameter_blocks, recon_gain_parameter_blocks, demixing_states,
      recon_gain_states));
}

bool ParametersManager::DemixingParamDefinitionAvailable(
    const DecodedUleb128 audio_element_id) const {
  return demixing_states_.find(audio_element_id) != demixing_states_.end();
}

// This function will populate `down_mixing_params` as follows:
// 1) If no parameter definition of type kParameterDefinitionDemixing
//    exists, it will use sensible defaults.
// 2) If such a parameter definition exists but no demixing parameter
//    blocks are present, it will use the parameter definition defaults.
// 3) If such a parameter definition exists AND a demixing parameter block
//    is present, it will use the values provided in the parameter block.
absl::Status ParametersManager::GetDownMixingParameters(
    const DecodedUleb128 audio_element_id,
    DownMixingParams& down_mixing_params) {
  const auto demixing_states_iter = demixing_states_.find(audio_element_id);
  if (demixing_states_iter == demixing_states_.end()) {
    if (ABSL_VLOG_IS_ON(1)) {
      ABSL_LOG_FIRST_N(WARNING, 1)
          << "No demixing parameter definition found for Audio "
          << "Element with ID= " << audio_element_id
          << "; using some sensible values.";
    }

    down_mixing_params = {
        0.707, 0.707, 0.707, 0.707, 0, 0, /*in_bitstream=*/false};
    return absl::OkStatus();
  }
  auto& demixing_state = demixing_states_iter->second;
  const auto* param_definition = demixing_state.param_definition;
  const auto* demixing_parameter_block =
      demixing_parameter_blocks_.at(param_definition->GetParameterId());
  if (demixing_parameter_block == nullptr) {
    // Failed to find a parameter block that overlaps this frame. Use the
    // default value from the parameter definition. This is OK when there are
    // no parameter blocks covering this substream. If there is only partial
    // coverage this will be marked invalid when the coverage of parameter
    // blocks is checked.
    ABSL_LOG_FIRST_N(WARNING, 10)
        << "Failed to find a parameter block; using the default values";
    RETURN_IF_NOT_OK(DemixingInfoParameterData::DMixPModeToDownMixingParams(
        param_definition->default_demixing_info_parameter_data_.dmixp_mode,
        param_definition->default_demixing_info_parameter_data_.default_w,
        DemixingInfoParameterData::kDefault, down_mixing_params));
    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(CompareTimestamps(
      demixing_state.next_timestamp, demixing_parameter_block->start_timestamp,
      absl::StrCat("Getting down-mixing parameters for audio element ID= ",
                   audio_element_id, ": ")));

  RETURN_IF_NOT_OK(DemixingInfoParameterData::DMixPModeToDownMixingParams(
      static_cast<DemixingInfoParameterData*>(
          demixing_parameter_block->obu->subblocks_[0].get())
          ->dmixp_mode,
      demixing_state.previous_w_idx, demixing_state.update_rule,
      down_mixing_params));
  demixing_state.w_idx = down_mixing_params.w_idx_used;
  return absl::OkStatus();
}

absl::Status ParametersManager::GetReconGainInfoParameterData(
    DecodedUleb128 audio_element_id, int32_t num_layers,
    ReconGainInfoParameterData& recon_gain_info_parameter_data) const {
  const auto recon_gain_states_iter = recon_gain_states_.find(audio_element_id);
  if (recon_gain_states_iter == recon_gain_states_.end()) {
    ABSL_LOG_FIRST_N(WARNING, 1)
        << "No recon gain parameter definition found for Audio "
        << "Element with ID= " << audio_element_id
        << "; setting recon gain to 255 (which represents a multiplier of 1.0, "
           "i.e. a gain of 0 dB) in all layers";
    for (int i = 0; i < num_layers; ++i) {
      ReconGainElement recon_gain_element;
      recon_gain_element.recon_gain_flag = 0;
      std::fill(recon_gain_element.recon_gain.begin(),
                recon_gain_element.recon_gain.end(), 255);
      recon_gain_info_parameter_data.recon_gain_elements.push_back(
          recon_gain_element);
    }
    return absl::OkStatus();
  }

  auto& recon_gain_state = recon_gain_states_iter->second;
  const auto* param_definition = recon_gain_state.param_definition;
  const auto* recon_gain_parameter_block =
      recon_gain_parameter_blocks_.at(param_definition->GetParameterId());
  if (recon_gain_parameter_block == nullptr) {
    // Failed to find a parameter block that overlaps this frame. A default
    // recon gain value of 0 dB is implied when there are no Parameter Block
    // OBUs provided. This is OK when there are no parameter blocks covering
    // this substream. If there is only partial coverage this will be marked
    // invalid when the coverage of parameter blocks is checked.
    ABSL_LOG_FIRST_N(WARNING, 10)
        << "Failed to find a recon gain parameter block; "
           "A default recon gain value of 0 dB is implied when there are no "
           "Parameter Block OBUs provided";

    for (int i = 0; i < num_layers; ++i) {
      ReconGainElement recon_gain_element;
      recon_gain_element.recon_gain_flag = 0;
      std::fill(recon_gain_element.recon_gain.begin(),
                recon_gain_element.recon_gain.end(), 255);
      recon_gain_info_parameter_data.recon_gain_elements.push_back(
          recon_gain_element);
    }

    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(CompareTimestamps(
      recon_gain_state.next_timestamp,
      recon_gain_parameter_block->start_timestamp,
      absl::StrCat("Getting recon gain parameters for audio element ID= ",
                   audio_element_id, ": ")));

  if (recon_gain_parameter_block->obu->subblocks_.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Recon gain parameter block for audio element ID= ", audio_element_id,
        " has no subblocks."));
  }
  auto recon_gain_info_parameter_data_in_obu =
      static_cast<ReconGainInfoParameterData*>(
          recon_gain_parameter_block->obu->subblocks_[0].get());
  // Verify that the element count matches the layer count (if any).
  if (num_layers > 0 &&
      recon_gain_info_parameter_data_in_obu->recon_gain_elements.size() !=
          static_cast<size_t>(num_layers)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Recon gain parameter block for audio element ID= ", audio_element_id,
        " has ",
        recon_gain_info_parameter_data_in_obu->recon_gain_elements.size(),
        " elements but the audio element has ", num_layers, " layers."));
  }
  recon_gain_info_parameter_data.recon_gain_elements =
      recon_gain_info_parameter_data_in_obu->recon_gain_elements;
  return absl::OkStatus();
}

void ParametersManager::AddDemixingParameterBlock(
    const ParameterBlockWithData* absl_nonnull parameter_block) {
  demixing_parameter_blocks_[parameter_block->obu->parameter_id_] =
      parameter_block;
}

void ParametersManager::AddReconGainParameterBlock(
    const ParameterBlockWithData* absl_nonnull parameter_block) {
  recon_gain_parameter_blocks_[parameter_block->obu->parameter_id_] =
      parameter_block;
}

absl::Status ParametersManager::UpdateDemixingState(
    DecodedUleb128 audio_element_id, InternalTimestamp expected_timestamp) {
  std::optional<DemixingState*> demixing_state = std::nullopt;
  RETURN_IF_NOT_OK(UpdateParameterState(
      audio_element_id, expected_timestamp, demixing_states_,
      demixing_parameter_blocks_, "down-mixing", demixing_state));

  // Additional updating steps to perform after `UpdateParameterState()`.
  if (demixing_state.has_value()) {
    // Update `previous_w_idx` for the next frame.
    (*demixing_state)->previous_w_idx = (*demixing_state)->w_idx;

    // Update the `update_rule` of the first frame to be "normally updating".
    if ((*demixing_state)->update_rule ==
        DemixingInfoParameterData::kFirstFrame) {
      (*demixing_state)->update_rule = DemixingInfoParameterData::kNormal;
    }
  }
  return absl::OkStatus();
}

absl::Status ParametersManager::UpdateReconGainState(
    DecodedUleb128 audio_element_id, InternalTimestamp expected_timestamp) {
  std::optional<ReconGainState*> recon_gain_state = std::nullopt;

  // No additional updating needed.
  return UpdateParameterState(audio_element_id, expected_timestamp,
                              recon_gain_states_, recon_gain_parameter_blocks_,
                              "down-mixing", recon_gain_state);
}

}  // namespace iamf_tools
