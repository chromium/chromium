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
#include "iamf/obu/param_definitions/param_definition_base.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ValidateSpecificParamDefinition(
    const ParamDefinition& param_definition) {
  using enum ParamDefinition::ParameterDefinitionType;
  switch (param_definition.GetType()) {
    case kParameterDefinitionDemixing:
    case kParameterDefinitionReconGain:
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.GetParamDefinitionMode(),
          ParamDefinition::kModeScheduleInParamDefinition,
          absl::StrCat("`param_definition_mode` for parameter_id= ",
                       param_definition.GetParameterId())));
      RETURN_IF_NOT_OK(
          ValidateNotEqual(param_definition.GetDuration(), DecodedUleb128{0},
                           absl::StrCat("duration for parameter_id= ",
                                        param_definition.GetParameterId())));
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.GetConstantSubblockDuration(),
          param_definition.GetDuration(),
          absl::StrCat("`constant_subblock_duration` for parameter_id= ",
                       param_definition.GetParameterId())));
      return absl::OkStatus();
    // Neither Mix gain nor Polar have any specific validation. For backwards
    // compatibility we must assume extension param definitions are valid as
    // well.
    case kParameterDefinitionMixGain:
    case kParameterDefinitionPolar:
    default:
      return absl::OkStatus();
  }
}

}  // namespace

DecodedUleb128 ParamDefinition::GetParameterId() const { return parameter_id_; }

DecodedUleb128 ParamDefinition::GetParameterRate() const {
  return parameter_rate_;
}

ParamDefinition::ParamDefinitionMode ParamDefinition::GetParamDefinitionMode()
    const {
  return schedule_.has_value() ? kModeScheduleInParamDefinition
                               : kModeScheduleInParameterBlock;
}

uint8_t ParamDefinition::GetReserved() const { return reserved_; }

// TODO(b/345799072): Determine how `GetDuration`,
//     `GetConstantSubblockDuration`, and  should behave
//     when the schedule is not set.

DecodedUleb128 ParamDefinition::GetDuration() const {
  return schedule_.has_value() ? schedule_->GetDuration() : 0;
}

DecodedUleb128 ParamDefinition::GetConstantSubblockDuration() const {
  return schedule_.has_value() ? schedule_->GetConstantSubblockDuration() : 0;
}

DecodedUleb128 ParamDefinition::GetNumSubblocks() const {
  return schedule_.has_value() ? schedule_->GetNumSubblocks() : 0;
}

const std::optional<SubblockSchedule>& ParamDefinition::GetSchedule() const {
  return schedule_;
}

absl::Status ParamDefinition::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(Validate());

  // Write the fields that are always present in `param_definition`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_rate_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(GetParamDefinitionMode(), 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 7));
  if (!schedule_.has_value()) {
    return absl::OkStatus();
  }

  // When the schedule is held within a parameter definition, the associated
  // data is not present.
  constexpr std::nullopt_t kDoNotWriteParameterData = std::nullopt;
  return schedule_->Write(kDoNotWriteParameterData, wb);
}

absl::Status ParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // Read the fields that are always present in `param_definition`.
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_id_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_rate_));
  uint8_t param_definition_mode;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, param_definition_mode));
  const auto mode = static_cast<ParamDefinitionMode>(param_definition_mode);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, reserved_));
  if (mode != kModeScheduleInParamDefinition) {
    schedule_ = std::nullopt;
    return absl::OkStatus();
  }

  auto temp_schedule = SubblockSchedule::CreateFromBuffer(rb);
  if (!temp_schedule.ok()) {
    return temp_schedule.status();
  }
  schedule_ = std::move(*temp_schedule);

  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

std::string ParamDefinition::ToString() const {
  std::string result = absl::StrCat(
      "  parameter_type= ", type_, "\n", "  parameter_id= ", parameter_id_,
      "\n", "  parameter_rate= ", parameter_rate_, "\n",
      "  param_definition_mode= ", GetParamDefinitionMode(), "\n",
      "  reserved= ", reserved_);
  if (schedule_.has_value()) {
    absl::StrAppend(&result, "\n", *schedule_);
  }
  return result;
}

absl::Status ParamDefinition::Validate() const {
  RETURN_IF_NOT_OK(
      ValidateNotEqual(parameter_rate_, DecodedUleb128{0}, "`parameter_rate`"));

  RETURN_IF_NOT_OK(ValidateSpecificParamDefinition(*this));

  return absl::OkStatus();
}

}  // namespace iamf_tools
