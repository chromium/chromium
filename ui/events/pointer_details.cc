// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/pointer_details.h"

#include <cmath>

namespace ui {

PointerDetails::PointerDetails() {}

PointerDetails::PointerDetails(EventPointerType pointer_type,
                               PointerId pointer_id)
    : PointerDetails(pointer_type,
                     pointer_id,
                     /* radius_x */ 0.0f,
                     /* radius_y */ 0.0f,
                     /* force */ std::numeric_limits<float>::quiet_NaN()) {}

PointerDetails::PointerDetails(EventPointerType pointer_type,
                               PointerId pointer_id,
                               float radius_x,
                               float radius_y,
                               float force,
                               float twist,
                               float tilt_x,
                               float tilt_y,
                               float tangential_pressure)
    : pointer_type(pointer_type),
      // If we aren't provided with a radius on one axis, use the
      // information from the other axis.
      radius_x(radius_x > 0 ? radius_x : radius_y),
      radius_y(radius_y > 0 ? radius_y : radius_x),
      force(force),
      tilt_x(tilt_x),
      tilt_y(tilt_y),
      tangential_pressure(tangential_pressure),
      twist(twist),
      id(pointer_id) {
  if (pointer_id == kPointerIdUnknown) {
    id = (pointer_type == EventPointerType::POINTER_TYPE_MOUSE)
             ? kPointerIdMouse
             : 0;
  }
}

PointerDetails::PointerDetails(const PointerDetails& other) = default;

bool PointerDetails::operator==(const PointerDetails& other) const {
  return pointer_type == other.pointer_type && radius_x == other.radius_x &&
         radius_y == other.radius_y &&
         (force == other.force ||
          (std::isnan(force) && std::isnan(other.force))) &&
         tilt_x == other.tilt_x && tilt_y == other.tilt_y &&
         tangential_pressure == other.tangential_pressure &&
         twist == other.twist && id == other.id && offset == other.offset;
}

}  // namespace ui
