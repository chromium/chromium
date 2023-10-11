// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_POINTER_DETAILS_H_
#define UI_EVENTS_POINTER_DETAILS_H_

#include <stdint.h>

#include <limits>

#include "ui/events/event_constants.h"
#include "ui/events/events_base_export.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

// Pointer ID type.
using PointerId = int32_t;

// Pointer id which means it needs to be initialized for all pointer types.
static constexpr PointerId kPointerIdUnknown = -1;
// Pointer id to be used by Mouse events.
static constexpr PointerId kPointerIdMouse =
    std::numeric_limits<int32_t>::max();

// Structure for handling common fields between touch and mouse to support
// PointerEvents API.
struct EVENTS_BASE_EXPORT PointerDetails {
 public:
  PointerDetails();
  explicit PointerDetails(EventPointerType pointer_type,
                          PointerId pointer_id = kPointerIdUnknown);
  PointerDetails(EventPointerType pointer_type,
                 PointerId pointer_id,
                 float radius_x,
                 float radius_y,
                 float force,
                 float twist = 0.0f,
                 double tilt_x = 0.0f,
                 double tilt_y = 0.0f,
                 float tangential_pressure = 0.0f);
  PointerDetails(const PointerDetails& other);
  PointerDetails& operator=(const PointerDetails& other);

  bool operator==(const PointerDetails& other) const;

  std::string ToString() const;

  // The type of pointer device.
  EventPointerType pointer_type = EventPointerType::kUnknown;

  // Radius of the X (major) axis of the touch ellipse. 0.0 if unknown.
  float radius_x = 0.0;

  // Radius of the Y (minor) axis of the touch ellipse. 0.0 if unknown.
  float radius_y = 0.0;

  // Force (pressure) of the touch. Normalized to be [0, 1] except NaN means
  // pressure is not supported by the input device.
  float force = 0.0;

  // Tilt of a pen/stylus from surface normal as plane angle in degrees, values
  // lie in [-90,90]. A positive tilt_x is to the right and a positive tilt_y
  // is towards the user. 0.0 if unknown.
  double tilt_x = 0.0;
  double tilt_y = 0.0;

  // The normalized tangential pressure (or barrel pressure), typically set by
  // an additional control of the stylus, which has a range of [-1,1], where 0
  // is the neutral position of the control. Always 0 if the device does not
  // support it.
  float tangential_pressure = 0.0;

  // The clockwise rotation of a pen stylus around its own major axis, in
  // degrees in the range [0,359]. Always 0 if the device does not support it.
  float twist = 0;

  // An identifier that uniquely identifies a pointer during its lifetime.
  PointerId id = 0;

  // Only used by mouse wheel events. The amount to scroll. This is in multiples
  // of kWheelDelta.
  // Note: offset_.x() > 0/offset_.y() > 0 means scroll left/up.
  gfx::Vector2d offset;

  // If you add fields please update ui/events/mojom/event.mojom.
};

}  // namespace ui

#endif  // UI_EVENTS_POINTER_DETAILS_H_
