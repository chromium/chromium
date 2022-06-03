// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "ui/display/display.h"
#include "ui/display/mojom/display.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    EnumTraits<display::mojom::Rotation, display::Display::Rotation> {
  static display::mojom::Rotation ToMojom(display::Display::Rotation type);
  static bool FromMojom(display::mojom::Rotation type,
                        display::Display::Rotation* output);
};

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    EnumTraits<display::mojom::TouchSupport, display::Display::TouchSupport> {
  static display::mojom::TouchSupport ToMojom(
      display::Display::TouchSupport type);
  static bool FromMojom(display::mojom::TouchSupport type,
                        display::Display::TouchSupport* output);
};

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    EnumTraits<display::mojom::AccelerometerSupport,
               display::Display::AccelerometerSupport> {
  static display::mojom::AccelerometerSupport ToMojom(
      display::Display::AccelerometerSupport type);
  static bool FromMojom(display::mojom::AccelerometerSupport type,
                        display::Display::AccelerometerSupport* output);
};

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    StructTraits<display::mojom::DisplayDataView, display::Display> {
  static int64_t id(const display::Display& display) { return display.id(); }

  static const gfx::Rect& bounds(const display::Display& display) {
    return display.bounds();
  }

  static gfx::Size size_in_pixels(const display::Display& display) {
    return display.GetSizeInPixel();
  }

  static const gfx::Rect& work_area(const display::Display& display) {
    return display.work_area();
  }

  static float device_scale_factor(const display::Display& display) {
    return display.device_scale_factor();
  }

  static display::Display::Rotation rotation(const display::Display& display) {
    return display.rotation();
  }

  static display::Display::TouchSupport touch_support(
      const display::Display& display) {
    return display.touch_support();
  }

  static display::Display::AccelerometerSupport accelerometer_support(
      const display::Display& display) {
    return display.accelerometer_support();
  }

  static const gfx::Size& maximum_cursor_size(const display::Display& display) {
    return display.maximum_cursor_size();
  }

  static gfx::DisplayColorSpaces color_spaces(const display::Display& display) {
    return display.color_spaces();
  }

  static int32_t color_depth(const display::Display& display) {
    return display.color_depth();
  }

  static int32_t depth_per_component(const display::Display& display) {
    return display.depth_per_component();
  }

  static bool is_monochrome(const display::Display& display) {
    return display.is_monochrome();
  }

  static int32_t display_frequency(const display::Display& display) {
    return display.display_frequency();
  }

  static bool Read(display::mojom::DisplayDataView data, display::Display* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_MOJOM_TRAITS_H_
