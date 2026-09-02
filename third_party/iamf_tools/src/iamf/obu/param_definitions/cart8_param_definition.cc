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
#include "iamf/obu/param_definitions/cart8_param_definition.h"

#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/cart8_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

absl::Status Cart8ParamDefinition::ValidateAndWrite(WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned8(default_x_));
  RETURN_IF_NOT_OK(wb.WriteSigned8(default_y_));
  RETURN_IF_NOT_OK(wb.WriteSigned8(default_z_));

  return absl::OkStatus();
}

absl::Status Cart8ParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_x_));
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_y_));
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_z_));
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
Cart8ParamDefinition::CreateParameterDataFromBuffer(ReadBitBuffer& rb) const {
  return absl::UnimplementedError(
      "CreateParameterDataFromBuffer for Cart8ParamDefinition is not "
      "implemented yet.");
}

void Cart8ParamDefinition::Print() const {
  ABSL_LOG(INFO) << "Cart8ParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  default_x: " << default_x_;
  ABSL_LOG(INFO) << "  default_y: " << default_y_;
  ABSL_LOG(INFO) << "  default_z: " << default_z_;
}

}  // namespace iamf_tools
