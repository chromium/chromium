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

#ifndef OBU_PARAM_DEFINITIONS_DUAL_CART8_PARAM_DEFINITION_H_
#define OBU_PARAM_DEFINITIONS_DUAL_CART8_PARAM_DEFINITION_H_

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

/*!\brief Parameter definition for polar info. */
class DualCart8ParamDefinition : public ParamDefinition {
 public:
  /*!\brief Constructor.
   *
   * \param base_args Arguments for `ParamDefinitionBase`.
   */
  explicit DualCart8ParamDefinition(const ParamDefinition::BaseArgs& base_args)
      : ParamDefinition(kParameterDefinitionDualCart8, base_args) {}

  /*!\brief Default destructor.
   */
  ~DualCart8ParamDefinition() override = default;

  friend bool operator==(const DualCart8ParamDefinition& lhs,
                         const DualCart8ParamDefinition& rhs) = default;

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
   * The created instance will be of type `DualCart8ParameterData`.
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

  int8_t default_first_x_ = 0;
  int8_t default_first_y_ = 0;
  int8_t default_first_z_ = 0;
  int8_t default_second_x_ = 0;
  int8_t default_second_y_ = 0;
  int8_t default_second_z_ = 0;
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_DUAL_CART8_PARAM_DEFINITION_H_
