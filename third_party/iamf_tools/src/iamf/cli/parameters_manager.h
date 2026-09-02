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

#ifndef CLI_PARAMETERS_MANAGER_H_
#define CLI_PARAMETERS_MANAGER_H_

#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Manages parameters and supports easy query.
 *
 * The class operates iteratively; holding one set of parameter blocks
 * corresponding to the same frame (with the same start/end timestamps).
 *
 * For each frame:
 *   - Parameter blocks are added via `AddDemixingParameterBlock()` or
 *     `AddReconGainParameterBlock()`.
 *   - Parameter values can be queried via `GetDownMixingParameters()` or
 *     `GetReconGainInfoParameterData()`.
 *   - Caller (usually the audio frame generator) is responsible to tell this
 *     manager to advance to the next frame via `UpdateDemixingState()` or
 *     `UpdateReconGainState()`.
 */
class ParametersManager {
 public:
  /*!\brief Factory function.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \return `ParametersManager` on success. A specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<ParametersManager> absl_nonnull> Create(
      const DescriptorObus::AudioElementsById& audio_elements);

  /*!\brief Checks if a `DemixingParamDefinition` exists for an audio element.
   *
   * \param audio_element_id ID of the audio element to query.
   * \return True if a `DemixingParamDefinition` is available for the audio
   *         element queried.
   */
  bool DemixingParamDefinitionAvailable(DecodedUleb128 audio_element_id) const;

  /*!\brief Gets current down-mixing parameters for an audio element.
   *
   * \param audio_element_id ID of the audio element that the parameters are
   *        to be applied.
   * \param down_mixing_params Output down mixing parameters.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDownMixingParameters(DecodedUleb128 audio_element_id,
                                       DownMixingParams& down_mixing_params);

  /*!\brief Gets current recon gain info parameter data for an audio element.
   *
   * \param audio_element_id ID of the audio element that the parameters are
   *        to be applied.
   * \param num_layers Number of layers in the audio element.
   * \param recon_gain_info_parameter_data Output recon gain info parameter
   *        data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetReconGainInfoParameterData(
      DecodedUleb128 audio_element_id, int32_t num_layers,
      ReconGainInfoParameterData& recon_gain_info_parameter_data) const;

  /*!\brief Adds a new demixing parameter block.
   *
   * \param parameter_block Pointer to the new demixing parameter block to add.
   */
  void AddDemixingParameterBlock(
      const ParameterBlockWithData* absl_nonnull parameter_block);

  /*!\brief Adds a new recon gain parameter block.
   *
   * \param parameter_block Pointer to the new recon gain parameter block to
   *        add.
   */
  void AddReconGainParameterBlock(
      const ParameterBlockWithData* absl_nonnull parameter_block);

  /*!\brief Updates the state of demixing parameters for an audio element.
   *
   * Also validates the timestamp is as expected.
   *
   * \param audio_element_id Audio Element ID whose corresponding demixing
   *        state are to be updated.
   * \param expected_next_timestamp Expected timestamp of the upcoming set of
   *        demixing parameter blocks.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateDemixingState(DecodedUleb128 audio_element_id,
                                   InternalTimestamp expected_next_timestamp);

  /*!\brief Updates the state of recon gain parameters for an audio element.
   *
   * Also validates the timestamp is as expected.
   *
   * \param audio_element_id Audio Element ID whose corresponding recon gain
   *        state are to be updated.
   * \param expected_new_timestamp Expected timestamp of the upcoming set of
   *        recon gain parameter blocks.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateReconGainState(DecodedUleb128 audio_element_id,
                                    InternalTimestamp expected_next_timestamp);

 private:
  // State used when generating demixing parameters for an audio element.
  struct DemixingState {
    const DemixingParamDefinition* absl_nonnull param_definition;

    // `w_idx` for the frame just processed, i.e. `wIdx(k - 1)` in the Spec.
    int previous_w_idx;

    // `w_idx` used to process the current frame, i.e. `wIdx(k)` in the Spec.
    int w_idx;

    // Timestamp for the next frame to be processed.
    InternalTimestamp next_timestamp;

    // Update rule of the currently tracked demixing parameters, because the
    // first frame needs some special treatment.
    DemixingInfoParameterData::WIdxUpdateRule update_rule;
  };

  struct ReconGainState {
    const ReconGainParamDefinition* absl_nonnull param_definition;

    // Timestamp for the next frame to be processed.
    InternalTimestamp next_timestamp;
  };

  /*!\brief Constructor.
   *
   * Used only by factory function.
   *
   * \param demixing_parameter_blocks Mapping from Parameter ID to demixing
   *        parameter blocks.
   * \param recon_gain_parameter_blocks Mapping from Parameter ID to recon gain
   *        parameter blocks.
   * \param demixing_states Mapping from Audio Element ID to the demixing
   *        state.
   * \param recon_gain_states Mapping from Audio Element ID to the recon gain
   *        state.
   */
  ParametersManager(
      absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
          demixing_parameter_blocks,
      absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
          recon_gain_parameter_blocks,
      absl::flat_hash_map<DecodedUleb128, DemixingState> demixing_states,
      absl::flat_hash_map<DecodedUleb128, ReconGainState> recon_gain_states)
      : demixing_parameter_blocks_(demixing_parameter_blocks),
        recon_gain_parameter_blocks_(recon_gain_parameter_blocks),
        demixing_states_(demixing_states),
        recon_gain_states_(recon_gain_states) {}

  // Mapping from Parameter ID to demixing parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      demixing_parameter_blocks_;

  // Mapping from Parameter ID to recon gain parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      recon_gain_parameter_blocks_;

  // Mapping from Audio Element ID to the demixing state.
  absl::flat_hash_map<DecodedUleb128, DemixingState> demixing_states_;

  // Mapping from Audio Element ID to the recon gain state.
  absl::flat_hash_map<DecodedUleb128, ReconGainState> recon_gain_states_;
};

}  // namespace iamf_tools

#endif  // CLI_PARAMETERS_MANAGER_H_
