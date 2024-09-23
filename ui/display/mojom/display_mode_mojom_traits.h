// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_MODE_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_MODE_MOJOM_TRAITS_H_

#include <memory>
#include <optional>

#include "ui/display/mojom/display_mode.mojom.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct StructTraits<display::mojom::DisplayModeDataView,
                    std::unique_ptr<display::DisplayMode>> {
  static const gfx::Size& size(
      const std::unique_ptr<display::DisplayMode>& display_mode) {
    return display_mode->size();
  }

  static bool is_interlaced(
      const std::unique_ptr<display::DisplayMode>& display_mode) {
    return display_mode->is_interlaced();
  }

  static float refresh_rate(
      const std::unique_ptr<display::DisplayMode>& display_mode) {
    return display_mode->refresh_rate();
  }

  static const std::optional<float>& vsync_rate_min(
      const std::unique_ptr<display::DisplayMode>& display_mode) {
    return display_mode->vsync_rate_min();
  }

  static bool IsNull(
      const std::unique_ptr<display::DisplayMode>& display_mode) {
    return !display_mode;
  }

  static void SetToNull(std::unique_ptr<display::DisplayMode>* output) {
    return output->reset();
  }

  static bool Read(display::mojom::DisplayModeDataView data,
                   std::unique_ptr<display::DisplayMode>* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_MODE_MOJOM_TRAITS_H_
