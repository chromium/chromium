/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_OBU_UTIL_H_
#define COMMON_UTILS_OBU_UTIL_H_

#include <cmath>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

// TODO(b/553298236): Support inter-linear and inter-bezier gains.
/*!\brief Interpolates a mix gain value in dB.
 *
 * The logic is used to partition parameter block protocol buffers as well as
 * to query the gain value at a specific timestamp during mixing.
 *
 * \param animation_type Type of animation applied to the mix gain values.
 * \param step_enum Enum value representing a step animation.
 * \param linear_enum Enum value representing a linear animation.
 * \param bezier_enum Enum value representing a Bezier animation.
 * \param step_start_point_getter Getter function of the start point value
 *        of a step animation.
 * \param linear_start_point_getter Getter function of the start point value
 *        of a linear animation.
 * \param linear_end_point_getter Getter function of the end point value
 *        of a linear animation.
 * \param bezier_start_point_getter Getter function of the start point value
 *        of a Bezier animation.
 * \param bezier_end_point_getter Getter function of the end point value
 *        of a Bezier animation.
 * \param bezier_control_point_getter Getter function of the middle control
 *        point value of a Bezier animation.
 * \param bezier_control_point_relative_time_getter Getter function of the
 *        time of the middle control point of a Bezier animation.
 * \param start_time Start time of the `MixGainParameterData`.
 * \param end_time End time of the `MixGainParameterData`.
 * \param target_time Target time to the get interpolated value of.
 * \param target_mix_gain_db Output inteprolated mix gain value in dB.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
template <typename AnimationEnumType>
absl::Status InterpolateMixGainValue(
    AnimationEnumType animation_type, AnimationEnumType step_enum,
    AnimationEnumType linear_enum, AnimationEnumType bezier_enum,
    absl::AnyInvocable<int16_t()> step_start_point_getter,
    absl::AnyInvocable<int16_t()> linear_start_point_getter,
    absl::AnyInvocable<int16_t()> linear_end_point_getter,
    absl::AnyInvocable<int16_t()> bezier_start_point_getter,
    absl::AnyInvocable<int16_t()> bezier_end_point_getter,
    absl::AnyInvocable<int16_t()> bezier_control_point_getter,
    absl::AnyInvocable<int16_t()> bezier_control_point_relative_time_getter,
    InternalTimestamp start_time, InternalTimestamp end_time,
    InternalTimestamp target_time, float& target_mix_gain_db) {
  if (target_time < start_time || target_time > end_time ||
      start_time > end_time) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot interpolate mix gain value with start time = ", start_time,
        ", target_time = ", target_time, " and end_time = ", end_time));
  }

  // Shift times so start_time=0 to simplify calculations.
  end_time -= start_time;
  target_time -= start_time;
  start_time = 0;

  // TODO(b/283281856): Support resampling parameter blocks.
  const int sample_rate_ratio = 1;
  const int n_0 = start_time * sample_rate_ratio;
  const int n = target_time * sample_rate_ratio;
  const int n_2 = end_time * sample_rate_ratio;

  if (animation_type == step_enum) {
    // No interpolation is needed for step.
    target_mix_gain_db = Q7_8ToFloat(step_start_point_getter());
  } else if (animation_type == linear_enum) {
    // Interpolate using the exact formula from the spec.
    const float a = (float)n / (float)n_2;
    const float p_0 = Q7_8ToFloat(linear_start_point_getter());
    const float p_2 = Q7_8ToFloat(linear_end_point_getter());
    target_mix_gain_db = (1 - a) * p_0 + a * p_2;
  } else if (animation_type == bezier_enum) {
    const float control_point_float =
        Q0_8ToFloat(bezier_control_point_relative_time_getter());
    // Using the definition of `round` in the IAMF spec.
    const int n_1 = std::floor((end_time * control_point_float) + 0.5);

    const float p_0 = Q7_8ToFloat(bezier_start_point_getter());
    const float p_1 = Q7_8ToFloat(bezier_control_point_getter());
    const float p_2 = Q7_8ToFloat(bezier_end_point_getter());

    const float alpha = n_0 - 2 * n_1 + n_2;
    const float beta = 2 * (n_1 - n_0);
    const float gamma = n_0 - n;
    const float a = alpha == 0
                        ? -gamma / beta
                        : (-beta + std::sqrt(beta * beta - 4 * alpha * gamma)) /
                              (2 * alpha);
    target_mix_gain_db =
        (1 - a) * (1 - a) * p_0 + 2 * (1 - a) * a * p_1 + a * a * p_2;
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown animation_type = ", animation_type));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // COMMON_UTILS_OBU_UTIL_H_
