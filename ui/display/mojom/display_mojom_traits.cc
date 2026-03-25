// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_mojom_traits.h"

#include "base/notreached.h"

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
  NOTREACHED();
}

display::Display::Rotation
EnumTraits<display::mojom::Rotation, display::Display::Rotation>::FromMojom(
    display::mojom::Rotation rotation) {
  switch (rotation) {
    case display::mojom::Rotation::VALUE_0:
      return display::Display::ROTATE_0;
    case display::mojom::Rotation::VALUE_90:
      return display::Display::ROTATE_90;
    case display::mojom::Rotation::VALUE_180:
      return display::Display::ROTATE_180;
    case display::mojom::Rotation::VALUE_270:
      return display::Display::ROTATE_270;
  }
  NOTREACHED();
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
  NOTREACHED();
}

display::Display::TouchSupport
EnumTraits<display::mojom::TouchSupport, display::Display::TouchSupport>::
    FromMojom(display::mojom::TouchSupport touch_support) {
  switch (touch_support) {
    case display::mojom::TouchSupport::UNKNOWN:
      return display::Display::TouchSupport::UNKNOWN;
    case display::mojom::TouchSupport::AVAILABLE:
      return display::Display::TouchSupport::AVAILABLE;
    case display::mojom::TouchSupport::UNAVAILABLE:
      return display::Display::TouchSupport::UNAVAILABLE;
  }
  NOTREACHED();
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
  NOTREACHED();
}

display::Display::AccelerometerSupport
EnumTraits<display::mojom::AccelerometerSupport,
           display::Display::AccelerometerSupport>::
    FromMojom(display::mojom::AccelerometerSupport accelerometer_support) {
  switch (accelerometer_support) {
    case display::mojom::AccelerometerSupport::UNKNOWN:
      return display::Display::AccelerometerSupport::UNKNOWN;
    case display::mojom::AccelerometerSupport::AVAILABLE:
      return display::Display::AccelerometerSupport::AVAILABLE;
    case display::mojom::AccelerometerSupport::UNAVAILABLE:
      return display::Display::AccelerometerSupport::UNAVAILABLE;
  }
  NOTREACHED();
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
