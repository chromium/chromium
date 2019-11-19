// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_SNAPSHOT_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_SNAPSHOT_MOJOM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "ui/display/mojom/display_constants_mojom_traits.h"
#include "ui/display/mojom/display_mode_mojom_traits.h"
#include "ui/display/mojom/display_snapshot.mojom.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<display::mojom::DisplaySnapshotDataView,
                    std::unique_ptr<display::DisplaySnapshot>> {
  static int64_t display_id(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->display_id();
  }

  static const gfx::Point& origin(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->origin();
  }

  static const gfx::Size& physical_size(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->physical_size();
  }

  static display::DisplayConnectionType type(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->type();
  }

  static display::PanelOrientation panel_orientation(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->panel_orientation();
  }

  static bool is_aspect_preserving_scaling(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->is_aspect_preserving_scaling();
  }

  static bool has_overscan(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->has_overscan();
  }

  static bool has_color_correction_matrix(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->has_color_correction_matrix();
  }

  static bool color_correction_in_linear_space(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->color_correction_in_linear_space();
  }

  static const gfx::ColorSpace& color_space(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->color_space();
  }

  static uint32_t bits_per_channel(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->bits_per_channel();
  }

  static std::string display_name(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->display_name();
  }

  static const base::FilePath& sys_path(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->sys_path();
  }

  static std::vector<uint8_t> edid(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->edid();
  }

  static std::vector<std::unique_ptr<display::DisplayMode>> modes(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot);

  static uint64_t current_mode_index(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot);

  static bool has_current_mode(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->current_mode() != nullptr;
  }

  static uint64_t native_mode_index(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot);

  static bool has_native_mode(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->native_mode() != nullptr;
  }

  static int64_t product_code(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->product_code();
  }

  static int32_t year_of_manufacture(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->year_of_manufacture();
  }

  static const gfx::Size& maximum_cursor_size(
      const std::unique_ptr<display::DisplaySnapshot>& snapshot) {
    return snapshot->maximum_cursor_size();
  }

  static bool Read(display::mojom::DisplaySnapshotDataView data,
                   std::unique_ptr<display::DisplaySnapshot>* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_SNAPSHOT_MOJOM_TRAITS_H_
