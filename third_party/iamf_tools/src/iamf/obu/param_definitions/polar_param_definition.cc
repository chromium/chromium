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
#include "iamf/obu/param_definitions/polar_param_definition.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/polar_parameter_data.h"

namespace iamf_tools {
namespace {

int16_t Clip3(int16_t value, int16_t min_val, int16_t max_val) {
  return std::clamp(value, min_val, max_val);
}

}  // namespace

absl::Status PolarParamDefinition::ValidateAndWrite(WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned9(Clip3(default_azimuth_, -180, 180)));
  RETURN_IF_NOT_OK(wb.WriteSigned8(Clip3(default_elevation_, -90, 90)));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(default_distance_, 7));

  return absl::OkStatus();
}

absl::Status PolarParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(rb.ReadSigned9(default_azimuth_));
  default_azimuth_ = Clip3(default_azimuth_, -180, 180);
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_elevation_));
  default_elevation_ = Clip3(default_elevation_, -90, 90);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, default_distance_));
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
PolarParamDefinition::CreateParameterDataFromBuffer(ReadBitBuffer& rb) const {
  return absl::UnimplementedError(
      "CreateParameterDataFromBuffer for PolarParamDefinition is not "
      "implemented yet.");
}

void PolarParamDefinition::Print() const {
  ABSL_LOG(INFO) << "PolarParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  default_azimuth: " << default_azimuth_;
  ABSL_LOG(INFO) << "  default_elevation: " << default_elevation_;
  ABSL_LOG(INFO) << "  default_distance: " << default_distance_;
}

}  // namespace iamf_tools
