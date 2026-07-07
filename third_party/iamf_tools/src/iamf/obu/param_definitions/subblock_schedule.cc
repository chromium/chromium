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
#include "iamf/obu/param_definitions/subblock_schedule.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

/*!\brief Helper function to parse a subblock schedule from a buffer.
 *
 * \param rb Buffer to read from.
 * \param on_subblock Callback function to be called for each subblock.
 * \return Validated SubblockSchedule or error status.
 */
absl::StatusOr<SubblockSchedule> ParseScheduleHelper(
    ReadBitBuffer& rb, std::function<absl::Status()> on_subblock) {
  DecodedUleb128 duration;
  DecodedUleb128 constant_subblock_duration;
  RETURN_IF_NOT_OK(rb.ReadULeb128(duration));
  RETURN_IF_NOT_OK(rb.ReadULeb128(constant_subblock_duration));

  if (constant_subblock_duration != 0) {
    auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(
        duration, constant_subblock_duration);
    if (!schedule.ok()) {
      return schedule.status();
    }
    // Ok. Subblocks are entirely after the schedule in the buffer.
    for (DecodedUleb128 i = 0; i < schedule->GetNumSubblocks(); ++i) {
      RETURN_IF_NOT_OK(on_subblock());
    }
    return schedule;
  }

  // Variable subblock duration.
  DecodedUleb128 num_subblocks;
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_subblocks));

  // Perform some early validation, to prevent excessive memory allocation.
  RETURN_IF_NOT_OK(ValidateNotEqual(duration, DecodedUleb128{0}, "duration"));
  RETURN_IF_NOT_OK(ValidateInRange(
      num_subblocks, {DecodedUleb128{1}, SubblockSchedule::kMaxNumSubblocks},
      "num_subblocks"));
  RETURN_IF_NOT_OK(Validate(num_subblocks, std::less_equal<DecodedUleb128>(),
                            duration, "num_subblocks < = duration"));

  // Read the subblock durations, which are interlaced with parameter data.
  std::vector<DecodedUleb128> subblock_durations(num_subblocks, 0);
  for (auto& subblock_duration : subblock_durations) {
    RETURN_IF_NOT_OK(rb.ReadULeb128(subblock_duration));
    RETURN_IF_NOT_OK(on_subblock());
  }

  auto schedule =
      SubblockSchedule::CreateWithVariableSubblockDuration(subblock_durations);
  if (!schedule.ok()) {
    return schedule.status();
  }
  RETURN_IF_NOT_OK(ValidateEqual(schedule->GetDuration(), duration,
                                 "Subblock durations must match the total "
                                 "duration."));
  return schedule;
}

}  // namespace

// TODO(b/345799072): Determine how `GetDuration`,
//     `GetConstantSubblockDuration`, and should behave when the schedule
//     represents unset states, or when special/optional fields are not present.

SubblockSchedule::SubblockSchedule(
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration,
    DecodedUleb128 num_subblocks,
    absl::Span<const DecodedUleb128> subblock_durations)
    : duration_(duration),
      constant_subblock_duration_(constant_subblock_duration),
      num_subblocks_(num_subblocks),
      subblock_durations_(subblock_durations.begin(),
                          subblock_durations.end()) {}

absl::StatusOr<SubblockSchedule> SubblockSchedule::CreateFromBuffer(
    ReadBitBuffer& rb) {
  const auto kDoNothingOnSubblock = []() { return absl::OkStatus(); };
  return ParseScheduleHelper(rb, kDoNothingOnSubblock);
}

absl::StatusOr<SubblockSchedule::ScheduleAndParameterData>
SubblockSchedule::CreateFromBufferWithParameterData(
    absl::FunctionRef<std::unique_ptr<ParameterData>()> create_parameter_data,
    ReadBitBuffer& rb) {
  std::vector<std::unique_ptr<ParameterData> absl_nonnull> parameter_data;

  const auto create_parameter_data_on_subblock = [&]() {
    auto param_data = create_parameter_data();
    RETURN_IF_NOT_OK(
        ValidateNotNull(param_data, "Failed to create parameter data."));
    RETURN_IF_NOT_OK(param_data->ReadAndValidate(rb));
    parameter_data.push_back(std::move(param_data));
    return absl::OkStatus();
  };

  auto schedule = ParseScheduleHelper(rb, create_parameter_data_on_subblock);
  if (!schedule.ok()) {
    return schedule.status();
  }

  return ScheduleAndParameterData{.schedule = *schedule,
                                  .parameter_data = std::move(parameter_data)};
}

absl::Status SubblockSchedule::Write(
    std::optional<absl::Span<const std::unique_ptr<ParameterData> absl_nonnull>>
        parameter_data,
    WriteBitBuffer& wb) const {
  if (parameter_data.has_value()) {
    RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
        "write_parameter_data", *parameter_data, num_subblocks_));
    for (const auto& data : *parameter_data) {
      ABSL_CHECK_NE(data, nullptr);
    }
  }
  RETURN_IF_NOT_OK(wb.WriteUleb128(duration_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(constant_subblock_duration_));
  if (constant_subblock_duration_ != 0) {
    if (parameter_data.has_value()) {
      for (DecodedUleb128 i = 0; i < num_subblocks_; ++i) {
        RETURN_IF_NOT_OK((*parameter_data)[i]->Write(wb));
      }
    }
    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(num_subblocks_));
  for (DecodedUleb128 i = 0; i < num_subblocks_; ++i) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(subblock_durations_[i]));
    if (parameter_data.has_value()) {
      RETURN_IF_NOT_OK((*parameter_data)[i]->Write(wb));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<SubblockSchedule>
SubblockSchedule::CreateWithConstantSubblockDuration(
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration) {
  if (duration == 0) {
    return absl::InvalidArgumentError("Duration should not be zero.");
  }
  if (constant_subblock_duration == 0) {
    return absl::InvalidArgumentError(
        "Constant subblock duration should not be zero.");
  }
  if (constant_subblock_duration > duration) {
    return absl::InvalidArgumentError(
        "Constant subblock duration should not be greater than duration.");
  }

  // Get the implicit value of `num_subblocks` using `ceil(duration /
  // constant_subblock_duration)`. Intentionally use integer division and ceil
  // based on the remainder to avoid floating point math.
  DecodedUleb128 num_subblocks = duration / constant_subblock_duration;
  if (duration % constant_subblock_duration != 0) {
    num_subblocks += 1;
  }
  RETURN_IF_NOT_OK(ValidateInRange(
      num_subblocks, {DecodedUleb128{1}, kMaxNumSubblocks}, "num_subblocks"));
  return SubblockSchedule(duration, constant_subblock_duration, num_subblocks,
                          {});
}

absl::StatusOr<SubblockSchedule>
SubblockSchedule::CreateWithVariableSubblockDuration(
    absl::Span<const DecodedUleb128> subblock_durations) {
  const DecodedUleb128 num_subblocks = subblock_durations.size();
  RETURN_IF_NOT_OK(ValidateInRange(
      num_subblocks, {DecodedUleb128{1}, kMaxNumSubblocks}, "num_subblocks"));

  uint32_t total_subblock_durations = 0;
  for (DecodedUleb128 i = 0; i < num_subblocks; i++) {
    if (subblock_durations[i] == 0) {
      // No individual subblock duration can be zero, and because there must be
      // at least one subblock, the total duration is non-zero and.
      return absl::InvalidArgumentError(
          absl::StrCat("Illegal zero duration for subblock[", i, "]."));
    }

    RETURN_IF_NOT_OK(AddUint32CheckOverflow(total_subblock_durations,
                                            subblock_durations[i],
                                            total_subblock_durations));
  }

  return SubblockSchedule(total_subblock_durations, 0, num_subblocks,
                          subblock_durations);
}

absl::StatusOr<DecodedUleb128> SubblockSchedule::GetSubblockDuration(
    int index) const {
  if (index < 0 || static_cast<DecodedUleb128>(index) >= num_subblocks_) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Index ", index, " out of range [0, ", num_subblocks_ - 1, "]."));
  }
  if (constant_subblock_duration_ == 0) {
    // The duration is explicitly set, the factory function should have ensured
    // the `subblock_durations_` has a size of `num_subblocks_`.
    ABSL_CHECK(static_cast<DecodedUleb128>(index) < subblock_durations_.size());
    return subblock_durations_[index];
  } else if (static_cast<DecodedUleb128>(index) == num_subblocks_ - 1) {
    // Special case of the last subblock duration under
    // `constant_subblock_duration_` mode.
    // The IAMF spec states: "If NS x CSD > D, the actual duration of the last
    // subblock SHALL be D - (NS - 1) x CSD."
    return duration_ - (num_subblocks_ - 1) * constant_subblock_duration_;
  } else {
    // The first `num_subblocks_ - 1` subblocks have the constant subblock
    // duration.
    return constant_subblock_duration_;
  }
}

void SubblockSchedule::Print() const {
  ABSL_LOG(INFO) << "  duration= " << duration_;
  ABSL_LOG(INFO) << "  constant_subblock_duration= "
                 << constant_subblock_duration_;
  ABSL_LOG(INFO) << "  num_subblocks= " << num_subblocks_;

  if (constant_subblock_duration_ == 0) {
    for (size_t i = 0; i < num_subblocks_; i++) {
      ABSL_LOG(INFO) << "  subblock_durations[" << i
                     << "]= " << subblock_durations_[i];
    }
  }
}

}  // namespace iamf_tools
