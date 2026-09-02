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
#include "iamf/obu/recon_gain_info_parameter_data.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<std::unique_ptr<ReconGainInfoParameterData>>
ReconGainInfoParameterData::CreateFromBuffer(
    ReadBitBuffer& rb,
    const std::vector<bool>& input_recon_gain_is_present_flags) {
  std::vector<std::optional<ReconGainElement>> recon_gain_elements;
  recon_gain_elements.resize(input_recon_gain_is_present_flags.size());
  for (size_t i = 0; i < input_recon_gain_is_present_flags.size(); i++) {
    auto& recon_gain_element = recon_gain_elements[i];

    if (!input_recon_gain_is_present_flags[i]) {
      recon_gain_element = std::nullopt;
      continue;
    }
    recon_gain_element = ReconGainElement();
    RETURN_IF_NOT_OK(rb.ReadULeb128(recon_gain_element->recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = recon_gain_element->recon_gain_flag;
    DecodedUleb128 mask = 1;

    for (size_t j = 0; j < recon_gain_element->recon_gain.size(); j++) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(
            rb.ReadUnsignedLiteral(8, recon_gain_element->recon_gain[j]));
      } else {
        recon_gain_element->recon_gain[j] = 0;
      }
      mask <<= 1;
    }
  }

  return Create(recon_gain_elements, input_recon_gain_is_present_flags);
}

absl::StatusOr<std::unique_ptr<ReconGainInfoParameterData>>
ReconGainInfoParameterData::Create(
    const std::vector<std::optional<ReconGainElement>>&
        input_recon_gain_elements,
    const std::vector<bool>& input_recon_gain_is_present_flags) {
  RETURN_IF_NOT_OK(ValidateEqual(input_recon_gain_elements.size(),
                                 input_recon_gain_is_present_flags.size(),
                                 "size of `recon_gain_elements`"));

  return std::unique_ptr<ReconGainInfoParameterData>(
      new ReconGainInfoParameterData(input_recon_gain_elements,
                                     input_recon_gain_is_present_flags));
}

absl::Status ReconGainInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateEqual(recon_gain_elements.size(),
                                 recon_gain_is_present_flags.size(),
                                 "size of `recon_gain_elements`"));

  for (size_t i = 0; i < recon_gain_is_present_flags.size(); i++) {
    // Each layer depends on the `recon_gain_is_present_flags` within the
    // associated Audio Element OBU.
    if (!recon_gain_is_present_flags[i]) {
      continue;
    }
    RETURN_IF_NOT_OK(ValidateHasValue(
        recon_gain_elements[i], absl::StrCat("recon_gain_elements[", i, "]")));
    const auto& recon_gain_element = *recon_gain_elements[i];
    RETURN_IF_NOT_OK(wb.WriteUleb128(recon_gain_element.recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = recon_gain_element.recon_gain_flag;
    DecodedUleb128 mask = 1;

    // Apply bitmask to examine each bit in the flag. Only write elements with
    // the flag implying they should be written.
    for (const auto& recon_gain : recon_gain_element.recon_gain) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(recon_gain, 8));
      }
      mask <<= 1;
    }
  }

  return absl::OkStatus();
}

void ReconGainInfoParameterData::Print() const {
  ABSL_LOG(INFO) << "  ReconGainInfoParameterData:";
  for (size_t l = 0; l < recon_gain_elements.size(); l++) {
    const auto& recon_gain_element = recon_gain_elements[l];
    ABSL_LOG(INFO) << "    recon_gain_elements[" << l << "]:";
    if (!recon_gain_element.has_value()) {
      ABSL_LOG(INFO) << "      NONE";
      continue;
    }
    ABSL_LOG(INFO) << "      recon_gain_flag= "
                   << recon_gain_element->recon_gain_flag;
    for (size_t b = 0; b < recon_gain_element->recon_gain.size(); b++) {
      ABSL_LOG(INFO) << "      recon_gain[" << b << "]= "
                     << absl::StrCat(recon_gain_element->recon_gain[b]);
    }
  }

  ABSL_LOG(INFO) << "    // recon_gain_is_present_flags: ";
  for (auto flag : recon_gain_is_present_flags) {
    ABSL_LOG(INFO) << "    //   " << absl::StrCat(flag);
  }
}

}  // namespace iamf_tools
