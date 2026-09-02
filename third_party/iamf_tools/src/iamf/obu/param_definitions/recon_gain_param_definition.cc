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
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {

absl::Status ReconGainParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // No sub-class specific part for Recon Gain Parameter Definition.

  return absl::OkStatus();
}

absl::Status ReconGainParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // No sub-class specific part for Recon Gain Parameter Definition.

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
ReconGainParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  std::vector<bool> recon_gain_is_present_flags(aux_data_.size());
  for (size_t i = 0; i < aux_data_.size(); ++i) {
    recon_gain_is_present_flags[i] = aux_data_[i].recon_gain_is_present_flag;
  }
  auto parameter_data = ReconGainInfoParameterData::CreateFromBuffer(
      rb, recon_gain_is_present_flags);
  if (!parameter_data.ok()) {
    return parameter_data.status();
  }
  return std::move(parameter_data.value());
}

void ReconGainParamDefinition::Print() const {
  ABSL_LOG(INFO) << "ReconGainParamDefinition:";
  ParamDefinition::Print();
  ABSL_LOG(INFO) << "  audio_element_id= " << audio_element_id_;

  for (size_t i = 0; i < aux_data_.size(); i++) {
    ABSL_LOG(INFO) << "  // recon_gain_is_present_flags[" << i << "]= "
                   << absl::StrCat(aux_data_[i].recon_gain_is_present_flag);
    const auto& channel_numbers = aux_data_[i].channel_numbers_for_layer;
    ABSL_LOG(INFO) << "  // channel_numbers_for_layer[" << i
                   << "]= " << channel_numbers.surround << "."
                   << channel_numbers.lfe << "." << channel_numbers.height;
  }
}

}  // namespace iamf_tools
