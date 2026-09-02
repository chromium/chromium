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
#include "iamf/obu/demixing_info_parameter_data.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

absl::Status DemixingInfoParameterData::DMixPModeToDownMixingParams(
    const DMixPMode dmixp_mode, const int previous_w_idx,
    const WIdxUpdateRule w_idx_update_rule,
    DownMixingParams& down_mixing_params) {
  static const absl::NoDestructor<
      absl::flat_hash_map<DMixPMode, DownMixingParams>>
      kDmixPModeToDownMixingParamValues(
          {{kDMixPMode1, {1, 1, 0.707, 0.707, -1, 0}},
           {kDMixPMode2, {0.707, 0.707, 0.707, 0.707, -1, 0}},
           {kDMixPMode3, {1, 0.866, 0.866, 0.866, -1, 0}},
           {kDMixPMode1_n, {1, 1, 0.707, 0.707, 1, 0}},
           {kDMixPMode2_n, {0.707, 0.707, 0.707, 0.707, 1, 0}},
           {kDMixPMode3_n, {1, 0.866, 0.866, 0.866, 1, 0}}});

  static const absl::NoDestructor<absl::flat_hash_map<int, double>>
      kWIdxToWValues({{0, 0},
                      {1, 0.0179},
                      {2, 0.0391},
                      {3, 0.0658},
                      {4, 0.1038},
                      {5, 0.25},
                      {6, 0.3962},
                      {7, 0.4342},
                      {8, 0.4609},
                      {9, 0.4821},
                      {10, 0.5}});

  const auto down_mixing_params_iter =
      kDmixPModeToDownMixingParamValues->find(dmixp_mode);
  if (down_mixing_params_iter == kDmixPModeToDownMixingParamValues->end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown dmixp_mode: ", dmixp_mode));
  }

  // According to the Spec, normally `wIdx` is updated to be
  // `Clip3(0, 10, wIdx(k - 1) + w_idx_offset(k))`.
  //
  // However, there are two special cases:
  // 1. If it is the first frame, then `wIdx(0) = 0`.
  // 2. If a parameter block is not found, then `default_w` (passed in as
  //    `previous_w_idx`) is used as `wIdx`.
  const int w_idx =
      w_idx_update_rule == DemixingInfoParameterData::kNormal
          ? std::clamp(
                previous_w_idx + down_mixing_params_iter->second.w_idx_offset,
                0, 10)
          : (w_idx_update_rule == DemixingInfoParameterData::kFirstFrame
                 ? 0
                 : previous_w_idx);

  const auto w_idx_iter = kWIdxToWValues->find(w_idx);
  if (w_idx_iter == kWIdxToWValues->end()) {
    return absl::InvalidArgumentError(absl::StrCat("Unknown w_idx: ", w_idx));
  }

  down_mixing_params = down_mixing_params_iter->second;
  down_mixing_params.w = w_idx_iter->second;
  down_mixing_params.w_idx_used = w_idx;
  down_mixing_params.in_bitstream = true;

  return absl::OkStatus();
}

absl::Status DemixingInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(dmixp_mode, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved, 5));

  // Validate that no reserved enums are used.
  switch (dmixp_mode) {
    case kDMixPMode1:
    case kDMixPMode2:
    case kDMixPMode3:
    case kDMixPMode1_n:
    case kDMixPMode2_n:
    case kDMixPMode3_n:
      return absl::OkStatus();
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported dmixp_mode= ", dmixp_mode));
  }
}

absl::StatusOr<std::unique_ptr<DemixingInfoParameterData>>
DemixingInfoParameterData::CreateFromBuffer(ReadBitBuffer& rb) {
  uint8_t dmixp_mode_int;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, dmixp_mode_int));
  const auto dmixp_mode = static_cast<DMixPMode>(dmixp_mode_int);
  uint8_t reserved;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, reserved));

  return Create(dmixp_mode, reserved);
}

absl::StatusOr<std::unique_ptr<DemixingInfoParameterData>>
DemixingInfoParameterData::Create(DMixPMode input_dmixp_mode,
                                  uint8_t input_reserved) {
  // Validate that no reserved enums are used.
  switch (input_dmixp_mode) {
    case kDMixPMode1:
    case kDMixPMode2:
    case kDMixPMode3:
    case kDMixPMode1_n:
    case kDMixPMode2_n:
    case kDMixPMode3_n:
      return std::unique_ptr<DemixingInfoParameterData>(
          new DemixingInfoParameterData(input_dmixp_mode, input_reserved));
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported dmixp_mode= ", input_dmixp_mode));
  }
}

void DemixingInfoParameterData::Print() const {
  ABSL_LOG(INFO) << "    dmixp_mode= " << absl::StrCat(dmixp_mode);
  ABSL_LOG(INFO) << "    reserved= " << absl::StrCat(reserved);
}

absl::Status DefaultDemixingInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(DemixingInfoParameterData::Write(wb));

  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(default_w, 4));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_for_future_use, 4));

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<DefaultDemixingInfoParameterData>>
DefaultDemixingInfoParameterData::CreateFromBuffer(ReadBitBuffer& rb) {
  // Read base class parts: dmixp_mode and reserved
  uint8_t dmixp_mode_int;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, dmixp_mode_int));
  const auto dmixp_mode =
      static_cast<DemixingInfoParameterData::DMixPMode>(dmixp_mode_int);
  uint8_t reserved;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, reserved));

  uint8_t default_w;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, default_w));
  uint8_t reserved_for_future_use;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, reserved_for_future_use));

  return Create(dmixp_mode, reserved, default_w, reserved_for_future_use);
}

absl::StatusOr<std::unique_ptr<DefaultDemixingInfoParameterData>>
DefaultDemixingInfoParameterData::Create(
    DMixPMode input_dmixp_mode, uint8_t input_reserved, uint8_t input_default_w,
    uint8_t input_reserved_for_future_use) {
  // Validate that no reserved enums are used.
  switch (input_dmixp_mode) {
    case kDMixPMode1:
    case kDMixPMode2:
    case kDMixPMode3:
    case kDMixPMode1_n:
    case kDMixPMode2_n:
    case kDMixPMode3_n:
      return std::unique_ptr<DefaultDemixingInfoParameterData>(
          new DefaultDemixingInfoParameterData(input_dmixp_mode, input_reserved,
                                               input_default_w,
                                               input_reserved_for_future_use));
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported dmixp_mode= ", input_dmixp_mode));
  }
}

void DefaultDemixingInfoParameterData::Print() const {
  DemixingInfoParameterData::Print();
  ABSL_LOG(INFO) << "    default_w= " << absl::StrCat(default_w);
  ABSL_LOG(INFO) << "    reserved_for_future_use= "
                 << absl::StrCat(reserved_for_future_use);
}

}  // namespace iamf_tools
