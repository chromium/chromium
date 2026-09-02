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
#ifndef OBU_PARAM_DEFINITIONS_DEMIXING_PARAM_DEFINITION_H_
#define OBU_PARAM_DEFINITIONS_DEMIXING_PARAM_DEFINITION_H_

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

/*!\brief Parameter definition for demixing info.
 */
class DemixingParamDefinition : public ParamDefinition {
 public:
  /*!\brief Constructor.
   *
   * \param base_args Arguments for `ParamDefinitionBase`.
   */
  explicit DemixingParamDefinition(const ParamDefinition::BaseArgs& base_args)
      : ParamDefinition(kParameterDefinitionDemixing, base_args) {}

  /*!\brief Default destructor.
   */
  ~DemixingParamDefinition() override = default;

  friend bool operator==(const DemixingParamDefinition& lhs,
                         const DemixingParamDefinition& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Creates parameter data from a buffer.
   *
   * The created instance will be of type `DemixingInfoParameterData`.
   *
   * \param rb Buffer to read from.
   * \return Unique pointer to created parameter data, or specific error
   *         on failure.
   */
  absl::StatusOr<std::unique_ptr<ParameterData>> CreateParameterDataFromBuffer(
      ReadBitBuffer& rb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  DefaultDemixingInfoParameterData default_demixing_info_parameter_data_;
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_DEMIXING_PARAM_DEFINITION_H_
