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
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/dual_polar_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {
namespace {

int16_t Clip3(int16_t value, int16_t min_val, int16_t max_val) {
  return std::clamp(value, min_val, max_val);
}

}  // namespace

absl::Status DualPolarParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned9(Clip3(default_first_azimuth_, -180, 180)));
  RETURN_IF_NOT_OK(wb.WriteSigned8(Clip3(default_first_elevation_, -90, 90)));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(default_first_distance_, 7));
  RETURN_IF_NOT_OK(wb.WriteSigned9(Clip3(default_second_azimuth_, -180, 180)));
  RETURN_IF_NOT_OK(wb.WriteSigned8(Clip3(default_second_elevation_, -90, 90)));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(default_second_distance_, 7));

  return absl::OkStatus();
}

absl::Status DualPolarParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(rb.ReadSigned9(default_first_azimuth_));
  default_first_azimuth_ = Clip3(default_first_azimuth_, -180, 180);
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_first_elevation_));
  default_first_elevation_ = Clip3(default_first_elevation_, -90, 90);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, default_first_distance_));
  RETURN_IF_NOT_OK(rb.ReadSigned9(default_second_azimuth_));
  default_second_azimuth_ = Clip3(default_second_azimuth_, -180, 180);
  RETURN_IF_NOT_OK(rb.ReadSigned8(default_second_elevation_));
  default_second_elevation_ = Clip3(default_second_elevation_, -90, 90);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, default_second_distance_));
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
DualPolarParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  return absl::UnimplementedError(
      "CreateParameterDataFromBuffer for DualPolarParamDefinition is not "
      "implemented yet.");
}

void DualPolarParamDefinition::Print() const {
  ABSL_LOG(INFO) << "DualPolarParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  default_first_azimuth: " << default_first_azimuth_;
  ABSL_LOG(INFO) << "  default_first_elevation: " << default_first_elevation_;
  ABSL_LOG(INFO) << "  default_first_distance: " << default_first_distance_;
  ABSL_LOG(INFO) << "  default_second_azimuth: " << default_second_azimuth_;
  ABSL_LOG(INFO) << "  default_second_elevation: " << default_second_elevation_;
  ABSL_LOG(INFO) << "  default_second_distance: " << default_second_distance_;
}

}  // namespace iamf_tools
