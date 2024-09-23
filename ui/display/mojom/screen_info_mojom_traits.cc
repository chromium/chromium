// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/screen_info_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/mojom/display_color_spaces.mojom.h"

namespace mojo {

bool StructTraits<display::mojom::ScreenInfoDataView, display::ScreenInfo>::
    Read(display::mojom::ScreenInfoDataView data, display::ScreenInfo* out) {
  if (!data.ReadDisplayColorSpaces(&out->display_color_spaces) ||
      !data.ReadRect(&out->rect) ||
      !data.ReadAvailableRect(&out->available_rect) ||
      !data.ReadLabel(&out->label)) {
    return false;
  }

  out->device_scale_factor = data.device_scale_factor();
  out->depth = data.depth();
  out->depth_per_component = data.depth_per_component();
  out->is_monochrome = data.is_monochrome();
  out->orientation_type = data.orientation_type();
  out->orientation_angle = data.orientation_angle();
  out->is_extended = data.is_extended();
  out->is_primary = data.is_primary();
  out->is_internal = data.is_internal();
  out->display_id = data.display_id();
  return true;
}

}  // namespace mojo
