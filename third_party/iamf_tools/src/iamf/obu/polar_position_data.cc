/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/obu/polar_position_data.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

int16_t Clip3(int16_t value, int16_t min_val, int16_t max_val) {
  return std::clamp(value, min_val, max_val);
}

absl::Status ValidatePosition(AnimationType animation_type,
                              const PolarPosition& position,
                              absl::string_view prefix) {
  RETURN_IF_NOT_OK(
      ValidateEqual(position.azimuth.animation_type(), animation_type,
                    absl::StrCat(prefix, " azimuth animation_type")));
  RETURN_IF_NOT_OK(
      ValidateEqual(position.elevation.animation_type(), animation_type,
                    absl::StrCat(prefix, " elevation animation_type")));
  RETURN_IF_NOT_OK(
      ValidateEqual(position.distance.animation_type(), animation_type,
                    absl::StrCat(prefix, " distance animation_type")));

  // azimuth is clipped to [-180, 180]
  RETURN_IF_NOT_OK(position.azimuth.ValidateRange(
      -180, 180, absl::StrCat(prefix, " azimuth")));
  // elevation is clipped to [-90, 90]
  RETURN_IF_NOT_OK(position.elevation.ValidateRange(
      -90, 90, absl::StrCat(prefix, " elevation")));
  // distance is 7 bits unsigned -> [0, 127]
  RETURN_IF_NOT_OK(position.distance.ValidateRange(
      0, 127, absl::StrCat(prefix, " distance")));
  return absl::OkStatus();
}

absl::StatusOr<PolarPosition> ReadPosition(AnimationType animation_type,
                                           ReadBitBuffer& rb) {
  auto read_azimuth = [](ReadBitBuffer& r, int16_t& val) -> absl::Status {
    RETURN_IF_NOT_OK(r.ReadSigned9(val));
    val = Clip3(val, -180, 180);
    return absl::OkStatus();
  };
  auto read_elevation = [](ReadBitBuffer& r, int8_t& val) -> absl::Status {
    RETURN_IF_NOT_OK(r.ReadSigned8(val));
    val = Clip3(val, -90, 90);
    return absl::OkStatus();
  };
  auto read_distance = [](ReadBitBuffer& r, uint8_t& val) {
    return r.ReadUnsignedLiteral(7, val);
  };

  auto azimuth =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_azimuth);
  if (!azimuth.ok()) {
    return azimuth.status();
  }

  auto elevation =
      AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_elevation);
  if (!elevation.ok()) {
    return elevation.status();
  }

  auto distance =
      AnimatedParameterData<uint8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_distance);
  if (!distance.ok()) {
    return distance.status();
  }

  return PolarPosition{
      .azimuth = *std::move(azimuth),
      .elevation = *std::move(elevation),
      .distance = *std::move(distance),
  };
}

}  // namespace

absl::StatusOr<PolarPositionData> PolarPositionData::CreateFromBuffer(
    ReadBitBuffer& rb, bool is_dual) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  const AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto first_position = ReadPosition(animation_type, rb);
  if (!first_position.ok()) {
    return first_position.status();
  }

  if (!is_dual) {
    return Create(animation_type, *std::move(first_position));
  }

  auto second_position = ReadPosition(animation_type, rb);
  if (!second_position.ok()) {
    return second_position.status();
  }

  return CreateDual(animation_type, *std::move(first_position),
                    *std::move(second_position));
}

absl::StatusOr<PolarPositionData> PolarPositionData::Create(
    AnimationType animation_type, PolarPosition first_position) {
  RETURN_IF_NOT_OK(
      ValidatePosition(animation_type, first_position, "first_position"));
  return PolarPositionData(animation_type, std::move(first_position),
                           /*second_position=*/std::nullopt);
}

absl::StatusOr<PolarPositionData> PolarPositionData::CreateDual(
    AnimationType animation_type, PolarPosition first_position,
    PolarPosition second_position) {
  RETURN_IF_NOT_OK(
      ValidatePosition(animation_type, first_position, "first_position"));
  RETURN_IF_NOT_OK(
      ValidatePosition(animation_type, second_position, "second_position"));
  return PolarPositionData(animation_type, std::move(first_position),
                           std::move(second_position));
}

absl::Status PolarPositionData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_azimuth = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned9(Clip3(val, -180, 180));
  };
  auto write_elevation = [](WriteBitBuffer& w, int8_t val) {
    return w.WriteSigned8(Clip3(val, -90, 90));
  };
  auto write_distance = [](WriteBitBuffer& w, uint8_t val) {
    return w.WriteUnsignedLiteral(val, 7);
  };

  auto write_position = [&](const PolarPosition& pos) -> absl::Status {
    RETURN_IF_NOT_OK(pos.azimuth.WritePayload(wb, write_azimuth));
    RETURN_IF_NOT_OK(pos.elevation.WritePayload(wb, write_elevation));
    RETURN_IF_NOT_OK(pos.distance.WritePayload(wb, write_distance));
    return absl::OkStatus();
  };

  RETURN_IF_NOT_OK(write_position(first_position_));
  if (second_position_.has_value()) {
    RETURN_IF_NOT_OK(write_position(*second_position_));
  }
  return absl::OkStatus();
}

std::string PolarPositionData::ToString() const {
  if (!second_position_.has_value()) {
    return absl::StrCat(
        "    animation_type= ", static_cast<DecodedUleb128>(animation_type_),
        "\n    azimuth:\n", first_position_.azimuth.ToStringPayload(),
        "\n    elevation:\n", first_position_.elevation.ToStringPayload(),
        "\n    distance:\n", first_position_.distance.ToStringPayload());
  }
  return absl::StrCat(
      "    animation_type= ", static_cast<DecodedUleb128>(animation_type_),
      "\n    first_azimuth:\n", first_position_.azimuth.ToStringPayload(),
      "\n    first_elevation:\n", first_position_.elevation.ToStringPayload(),
      "\n    first_distance:\n", first_position_.distance.ToStringPayload(),
      "\n    second_azimuth:\n", second_position_->azimuth.ToStringPayload(),
      "\n    second_elevation:\n",
      second_position_->elevation.ToStringPayload(), "\n    second_distance:\n",
      second_position_->distance.ToStringPayload());
}

}  // namespace iamf_tools
