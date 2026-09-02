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
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"

#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/dual_cart16_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

absl::Status DualCart16ParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_first_x_));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_first_y_));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_first_z_));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_second_x_));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_second_y_));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_second_z_));

  return absl::OkStatus();
}

absl::Status DualCart16ParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_first_x_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_first_y_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_first_z_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_second_x_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_second_y_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_second_z_));
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
DualCart16ParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  return absl::UnimplementedError(
      "CreateParameterDataFromBuffer for DualCart16ParamDefinition is not "
      "implemented yet.");
}

void DualCart16ParamDefinition::Print() const {
  ABSL_LOG(INFO) << "DualCart16ParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  default_first_x: " << default_first_x_;
  ABSL_LOG(INFO) << "  default_first_y: " << default_first_y_;
  ABSL_LOG(INFO) << "  default_first_z: " << default_first_z_;
  ABSL_LOG(INFO) << "  default_second_x: " << default_second_x_;
  ABSL_LOG(INFO) << "  default_second_y: " << default_second_y_;
  ABSL_LOG(INFO) << "  default_second_z: " << default_second_z_;
}

}  // namespace iamf_tools
