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

#include "iamf/obu/cartesian_position_data.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
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

absl::Status ValidatePosition(AnimationType animation_type,
                              CartesianBitDepth bit_depth,
                              const CartesianPosition& position,
                              absl::string_view prefix) {
  RETURN_IF_NOT_OK(ValidateEqual(position.x.animation_type(), animation_type,
                                 absl::StrCat(prefix, " x animation_type")));
  RETURN_IF_NOT_OK(ValidateEqual(position.y.animation_type(), animation_type,
                                 absl::StrCat(prefix, " y animation_type")));
  RETURN_IF_NOT_OK(ValidateEqual(position.z.animation_type(), animation_type,
                                 absl::StrCat(prefix, " z animation_type")));

  int16_t min_val = -32768;
  int16_t max_val = 32767;
  if (bit_depth == CartesianBitDepth::k8Bit) {
    min_val = -128;
    max_val = 127;
  }

  RETURN_IF_NOT_OK(
      position.x.ValidateRange(min_val, max_val, absl::StrCat(prefix, " x")));
  RETURN_IF_NOT_OK(
      position.y.ValidateRange(min_val, max_val, absl::StrCat(prefix, " y")));
  RETURN_IF_NOT_OK(
      position.z.ValidateRange(min_val, max_val, absl::StrCat(prefix, " z")));
  return absl::OkStatus();
}

absl::StatusOr<CartesianPosition> ReadPosition(AnimationType animation_type,
                                               CartesianBitDepth bit_depth,
                                               ReadBitBuffer& rb) {
  auto read_coord = [bit_depth](ReadBitBuffer& r,
                                int16_t& val) -> absl::Status {
    if (bit_depth == CartesianBitDepth::k8Bit) {
      int8_t val_8;
      RETURN_IF_NOT_OK(r.ReadSigned8(val_8));
      val = static_cast<int16_t>(val_8);
      return absl::OkStatus();
    } else {
      return r.ReadSigned16(val);
    }
  };

  ABSL_ASSIGN_OR_RETURN(
      auto x,
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_coord));
  ABSL_ASSIGN_OR_RETURN(
      auto y,
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_coord));
  ABSL_ASSIGN_OR_RETURN(
      auto z,
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_coord));

  return CartesianPosition{
      .x = std::move(x),
      .y = std::move(y),
      .z = std::move(z),
  };
}

}  // namespace

absl::StatusOr<CartesianPositionData> CartesianPositionData::CreateFromBuffer(
    ReadBitBuffer& rb, CartesianBitDepth bit_depth, bool is_dual) {
  if (bit_depth != CartesianBitDepth::k8Bit &&
      bit_depth != CartesianBitDepth::k16Bit) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid bit_depth: ", static_cast<int>(bit_depth)));
  }

  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));
  const AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto first_position = ReadPosition(animation_type, bit_depth, rb);
  if (!first_position.ok()) {
    return first_position.status();
  }

  if (!is_dual) {
    return Create(animation_type, bit_depth, *std::move(first_position));
  }

  auto second_position = ReadPosition(animation_type, bit_depth, rb);
  if (!second_position.ok()) {
    return second_position.status();
  }

  return CreateDual(animation_type, bit_depth, *std::move(first_position),
                    *std::move(second_position));
}

absl::StatusOr<CartesianPositionData> CartesianPositionData::Create(
    AnimationType animation_type, CartesianBitDepth bit_depth,
    CartesianPosition first_position) {
  if (bit_depth != CartesianBitDepth::k8Bit &&
      bit_depth != CartesianBitDepth::k16Bit) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid bit_depth: ", static_cast<int>(bit_depth)));
  }
  RETURN_IF_NOT_OK(ValidatePosition(animation_type, bit_depth, first_position,
                                    "first_position"));
  return CartesianPositionData(animation_type, bit_depth,
                               std::move(first_position), std::nullopt);
}

absl::StatusOr<CartesianPositionData> CartesianPositionData::CreateDual(
    AnimationType animation_type, CartesianBitDepth bit_depth,
    CartesianPosition first_position, CartesianPosition second_position) {
  if (bit_depth != CartesianBitDepth::k8Bit &&
      bit_depth != CartesianBitDepth::k16Bit) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid bit_depth: ", static_cast<int>(bit_depth)));
  }
  RETURN_IF_NOT_OK(ValidatePosition(animation_type, bit_depth, first_position,
                                    "first_position"));
  RETURN_IF_NOT_OK(ValidatePosition(animation_type, bit_depth, second_position,
                                    "second_position"));
  return CartesianPositionData(animation_type, bit_depth,
                               std::move(first_position),
                               std::move(second_position));
}

absl::Status CartesianPositionData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_coord = [this](WriteBitBuffer& w, int16_t val) -> absl::Status {
    if (bit_depth_ == CartesianBitDepth::k8Bit) {
      return w.WriteSigned8(static_cast<int8_t>(val));
    } else {
      return w.WriteSigned16(val);
    }
  };

  auto write_position = [&](const CartesianPosition& pos) -> absl::Status {
    RETURN_IF_NOT_OK(pos.x.WritePayload(wb, write_coord));
    RETURN_IF_NOT_OK(pos.y.WritePayload(wb, write_coord));
    RETURN_IF_NOT_OK(pos.z.WritePayload(wb, write_coord));
    return absl::OkStatus();
  };

  RETURN_IF_NOT_OK(write_position(first_position_));
  if (second_position_.has_value()) {
    RETURN_IF_NOT_OK(write_position(*second_position_));
  }
  return absl::OkStatus();
}

std::string CartesianPositionData::ToString() const {
  if (!second_position_.has_value()) {
    return absl::StrCat(
        "    animation_type= ", static_cast<DecodedUleb128>(animation_type_),
        "\n    x:\n", first_position_.x.ToStringPayload(), "\n    y:\n",
        first_position_.y.ToStringPayload(), "\n    z:\n",
        first_position_.z.ToStringPayload());
  }
  return absl::StrCat(
      "    animation_type= ", static_cast<DecodedUleb128>(animation_type_),
      "\n    first_x:\n", first_position_.x.ToStringPayload(),
      "\n    first_y:\n", first_position_.y.ToStringPayload(),
      "\n    first_z:\n", first_position_.z.ToStringPayload(),
      "\n    second_x:\n", second_position_->x.ToStringPayload(),
      "\n    second_y:\n", second_position_->y.ToStringPayload(),
      "\n    second_z:\n", second_position_->z.ToStringPayload());
}

}  // namespace iamf_tools
