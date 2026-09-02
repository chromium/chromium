/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

absl::Status MixGainParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_mix_gain_.GetQ7_8()));
  return absl::OkStatus();
}

absl::Status MixGainParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  int16_t default_mix_gain_q78;
  RETURN_IF_NOT_OK(rb.ReadSigned16(default_mix_gain_q78));
  default_mix_gain_ =
      QFormatOrFloatingPoint::MakeFromQ7_8(default_mix_gain_q78);
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
MixGainParamDefinition::CreateParameterDataFromBuffer(ReadBitBuffer& rb) const {
  auto parameter_data = MixGainParameterData::CreateFromBuffer(rb);
  if (!parameter_data.ok()) {
    return parameter_data.status();
  }
  return std::make_unique<MixGainParameterData>(
      std::move(parameter_data.value()));
}

void MixGainParamDefinition::Print() const {
  ABSL_LOG(INFO) << "MixGainParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  default_mix_gain= " << default_mix_gain_;
}

}  // namespace iamf_tools
