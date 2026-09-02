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
#include "iamf/obu/param_definitions/demixing_param_definition.h"

#include <memory>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

absl::Status DemixingParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(default_demixing_info_parameter_data_.Write(wb));

  return absl::OkStatus();
}

absl::Status DemixingParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  auto default_demixing_param_data =
      DefaultDemixingInfoParameterData::CreateFromBuffer(rb);
  if (!default_demixing_param_data.ok()) {
    return default_demixing_param_data.status();
  }
  default_demixing_info_parameter_data_ =
      std::move(**default_demixing_param_data);

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
DemixingParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  auto parameter_data = DemixingInfoParameterData::CreateFromBuffer(rb);
  if (!parameter_data.ok()) {
    return parameter_data.status();
  }
  return std::move(parameter_data.value());
}

void DemixingParamDefinition::Print() const {
  ABSL_LOG(INFO) << "DemixingParamDefinition:";
  ParamDefinition::Print();
  default_demixing_info_parameter_data_.Print();
}

}  // namespace iamf_tools
