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
#include "iamf/obu/param_definitions/extended_param_definition.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/extension_parameter_data.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::Status ExtendedParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // This class does not write the base class's data, i.e. it doesn't call
  // `ParamDefinition::ValidateAndWrite(wb)`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(param_definition_bytes_.size()));
  RETURN_IF_NOT_OK(
      wb.WriteUint8Span(absl::MakeConstSpan(param_definition_bytes_)));

  return absl::OkStatus();
}

absl::Status ExtendedParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // This class does not read the base class's data, i.e. it doesn't call
  // `ParamDefinition::ReadAndWrite(wb)`.
  DecodedUleb128 param_definition_size;
  RETURN_IF_NOT_OK(rb.ReadULeb128(param_definition_size));
  if (param_definition_size > kEntireObuSizeMaxTwoMegabytes) {
    return absl::InvalidArgumentError(absl::StrCat(
        "param_definition_size= ", param_definition_size, " exceeds maximum."));
  }
  param_definition_bytes_.resize(param_definition_size);
  RETURN_IF_NOT_OK(rb.ReadUint8Span(absl::MakeSpan(param_definition_bytes_)));

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
ExtendedParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  auto parameter_data = ExtensionParameterData::CreateFromBuffer(rb);
  if (!parameter_data.ok()) {
    return parameter_data.status();
  }
  return std::move(parameter_data.value());
}

void ExtendedParamDefinition::Print() const {
  ABSL_LOG(INFO) << "ExtendedParamDefinition:";
  // This class does not read the base class's data, i.e. it doesn't call
  // `ParamDefinition::Print()`.
  ABSL_LOG(INFO) << "  param_definition_size= "
                 << param_definition_bytes_.size();
  ABSL_LOG(INFO) << "  // Skipped printing param_definition_bytes";
}

}  // namespace iamf_tools
