// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_mojom_traits.h"

namespace mojo {

display::mojom::Rotation
EnumTraits<display::mojom::Rotation, display::Display::Rotation>::ToMojom(
    display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return display::mojom::Rotation::VALUE_0;
    case display::Display::ROTATE_90:
      return display::mojom::Rotation::VALUE_90;
    case display::Display::ROTATE_180:
      return display::mojom::Rotation::VALUE_180;
    case display::Display::ROTATE_270:
      return display::mojom::Rotation::VALUE_270;
  }
  NOTREACHED_IN_MIGRATION();
  return display::mojom::Rotation::VALUE_0;
}

bool EnumTraits<display::mojom::Rotation, display::Display::Rotation>::
    FromMojom(display::mojom::Rotation rotation,
              display::Display::Rotation* out) {
  switch (rotation) {
    case display::mojom::Rotation::VALUE_0:
      *out = display::Display::ROTATE_0;
      return true;
    case display::mojom::Rotation::VALUE_90:
      *out = display::Display::ROTATE_90;
      return true;
    case display::mojom::Rotation::VALUE_180:
      *out = display::Display::ROTATE_180;
      return true;
    case display::mojom::Rotation::VALUE_270:
      *out = display::Display::ROTATE_270;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

display::mojom::TouchSupport
EnumTraits<display::mojom::TouchSupport, display::Display::TouchSupport>::
    ToMojom(display::Display::TouchSupport touch_support) {
  switch (touch_support) {
    case display::Display::TouchSupport::UNKNOWN:
      return display::mojom::TouchSupport::UNKNOWN;
    case display::Display::TouchSupport::AVAILABLE:
      return display::mojom::TouchSupport::AVAILABLE;
    case display::Display::TouchSupport::UNAVAILABLE:
      return display::mojom::TouchSupport::UNAVAILABLE;
  }
  NOTREACHED_IN_MIGRATION();
  return display::mojom::TouchSupport::UNKNOWN;
}

bool EnumTraits<display::mojom::TouchSupport, display::Display::TouchSupport>::
    FromMojom(display::mojom::TouchSupport touch_support,
              display::Display::TouchSupport* out) {
  switch (touch_support) {
    case display::mojom::TouchSupport::UNKNOWN:
      *out = display::Display::TouchSupport::UNKNOWN;
      return true;
    case display::mojom::TouchSupport::AVAILABLE:
      *out = display::Display::TouchSupport::AVAILABLE;
      return true;
    case display::mojom::TouchSupport::UNAVAILABLE:
      *out = display::Display::TouchSupport::UNAVAILABLE;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

display::mojom::AccelerometerSupport
EnumTraits<display::mojom::AccelerometerSupport,
           display::Display::AccelerometerSupport>::
    ToMojom(display::Display::AccelerometerSupport accelerometer_support) {
  switch (accelerometer_support) {
    case display::Display::AccelerometerSupport::UNKNOWN:
      return display::mojom::AccelerometerSupport::UNKNOWN;
    case display::Display::AccelerometerSupport::AVAILABLE:
      return display::mojom::AccelerometerSupport::AVAILABLE;
    case display::Display::AccelerometerSupport::UNAVAILABLE:
      return display::mojom::AccelerometerSupport::UNAVAILABLE;
  }
  NOTREACHED_IN_MIGRATION();
  return display::mojom::AccelerometerSupport::UNKNOWN;
}

bool EnumTraits<display::mojom::AccelerometerSupport,
                display::Display::AccelerometerSupport>::
    FromMojom(display::mojom::AccelerometerSupport accelerometer_support,
              display::Display::AccelerometerSupport* out) {
  switch (accelerometer_support) {
    case display::mojom::AccelerometerSupport::UNKNOWN:
      *out = display::Display::AccelerometerSupport::UNKNOWN;
      return true;
    case display::mojom::AccelerometerSupport::AVAILABLE:
      *out = display::Display::AccelerometerSupport::AVAILABLE;
      return true;
    case display::mojom::AccelerometerSupport::UNAVAILABLE:
      *out = display::Display::AccelerometerSupport::UNAVAILABLE;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<display::mojom::DisplayDataView, display::Display>::Read(
    display::mojom::DisplayDataView data,
    display::Display* out) {
  out->set_id(data.id());

  if (!data.ReadBounds(&out->bounds_))
    return false;

  if (!data.ReadSizeInPixels(&out->size_in_pixels_))
    return false;

  if (!data.ReadNativeOrigin(&out->native_origin_)) {
    return false;
  }

  if (!data.ReadWorkArea(&out->work_area_))
    return false;

  out->set_device_scale_factor(data.device_scale_factor());

  if (!data.ReadRotation(&out->rotation_))
    return false;

  if (!data.ReadTouchSupport(&out->touch_support_))
    return false;

  if (!data.ReadAccelerometerSupport(&out->accelerometer_support_))
    return false;

  if (!data.ReadMaximumCursorSize(&out->maximum_cursor_size_))
    return false;

  gfx::DisplayColorSpaces color_spaces = out->GetColorSpaces();

  if (!data.ReadColorSpaces(&color_spaces)) {
    return false;
  }

  out->SetColorSpaces(color_spaces);
  out->set_color_depth(data.color_depth());
  out->set_depth_per_component(data.depth_per_component());
  out->set_is_monochrome(data.is_monochrome());
  out->set_display_frequency(data.display_frequency());

  if (!data.ReadLabel(&out->label_))
    return false;

  return true;
}

}  // namespace mojo
