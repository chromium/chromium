// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_SCREEN_INFO_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_SCREEN_INFO_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "ui/display/mojom/screen_info.mojom-shared.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/display/screen_info.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(DISPLAY_SHARED_MOJOM_TRAITS)
    StructTraits<display::mojom::ScreenInfoDataView, display::ScreenInfo> {
  static float device_scale_factor(const display::ScreenInfo& r) {
    return r.device_scale_factor;
  }

  static const gfx::DisplayColorSpaces& display_color_spaces(
      const display::ScreenInfo& r) {
    return r.display_color_spaces;
  }

  static int depth(const display::ScreenInfo& r) { return r.depth; }

  static int depth_per_component(const display::ScreenInfo& r) {
    return r.depth_per_component;
  }

  static bool is_monochrome(const display::ScreenInfo& r) {
    return r.is_monochrome;
  }

  static const gfx::Rect& rect(const display::ScreenInfo& r) { return r.rect; }

  static const gfx::Rect& available_rect(const display::ScreenInfo& r) {
    return r.available_rect;
  }

  static display::mojom::ScreenOrientation orientation_type(
      const display::ScreenInfo& r) {
    return r.orientation_type;
  }

  static uint16_t orientation_angle(const display::ScreenInfo& r) {
    return r.orientation_angle;
  }

  static bool is_extended(const display::ScreenInfo& r) {
    return r.is_extended;
  }

  static bool is_primary(const display::ScreenInfo& r) { return r.is_primary; }

  static bool is_internal(const display::ScreenInfo& r) {
    return r.is_internal;
  }

  static const std::string& label(const display::ScreenInfo& r) {
    return r.label;
  }

  static int64_t display_id(const display::ScreenInfo& r) {
    return r.display_id;
  }

  static bool Read(display::mojom::ScreenInfoDataView r,
                   display::ScreenInfo* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_SCREEN_INFO_MOJOM_TRAITS_H_
