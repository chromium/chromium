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
#include "iamf/obu/extension_parameter_data.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

absl::Status ValidateSize(DecodedUleb128 size) {
  if (size > kEntireObuSizeMaxTwoMegabytes) {
    return absl::InvalidArgumentError(absl::StrCat(
        "parameter_data_size= ", size, " exceeds maximum OBU size."));
  }
  return absl::OkStatus();
}

}  // namespace
absl::StatusOr<std::unique_ptr<ExtensionParameterData>>
ExtensionParameterData::CreateFromBuffer(ReadBitBuffer& rb) {
  DecodedUleb128 parameter_data_size;
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_data_size));
  RETURN_IF_NOT_OK(ValidateSize(parameter_data_size));
  std::vector<uint8_t> parameter_data_bytes;
  parameter_data_bytes.resize(parameter_data_size);
  RETURN_IF_NOT_OK(rb.ReadUint8Span(absl::MakeSpan(parameter_data_bytes)));
  return Create(parameter_data_bytes);
}

absl::StatusOr<std::unique_ptr<ExtensionParameterData>>
ExtensionParameterData::Create(
    absl::Span<const uint8_t> input_parameter_data_bytes) {
  RETURN_IF_NOT_OK(ValidateSize(input_parameter_data_bytes.size()));
  return std::unique_ptr<ExtensionParameterData>(
      new ExtensionParameterData(input_parameter_data_bytes));
}

absl::Status ExtensionParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_data_bytes.size()));
  return wb.WriteUint8Span(absl::MakeConstSpan(parameter_data_bytes));
}

void ExtensionParameterData::Print() const {
  ABSL_LOG(INFO) << "    parameter_data_size= "
                 << absl::StrCat(parameter_data_bytes.size());
  ABSL_LOG(INFO) << "    // parameter_data_bytes.size()= "
                 << absl::StrCat(parameter_data_bytes.size());
}

}  // namespace iamf_tools
