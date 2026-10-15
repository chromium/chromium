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
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/cartesian_position_data.h"
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
  auto data = CartesianPositionData::CreateFromBuffer(
      rb, CartesianBitDepth::k8Bit, /*is_dual=*/false);
  if (!data.ok()) {
    return data.status();
  }
  return std::make_unique<CartesianPositionData>(*std::move(data));
}

std::string Cart8ParamDefinition::ToString() const {
  return absl::StrCat("Cart8ParamDefinition:\n", ParamDefinition::ToString(),
                      "\n  default_x: ", default_x_,
                      "\n  default_y: ", default_y_,
                      "\n  default_z: ", default_z_);
}

}  // namespace iamf_tools
