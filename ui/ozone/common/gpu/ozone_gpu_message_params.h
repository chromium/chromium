// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
#define UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
class OverlaySurfaceCandidate;

struct DisplayMode_Params {
  DisplayMode_Params();
  ~DisplayMode_Params();

  gfx::Size size;
  bool is_interlaced = false;
  float refresh_rate = 0.0f;
};

struct DisplaySnapshot_Params {
  DisplaySnapshot_Params();
  DisplaySnapshot_Params(const DisplaySnapshot_Params& other);
  ~DisplaySnapshot_Params();

  int64_t display_id = 0;
  gfx::Point origin;
  gfx::Size physical_size;
  display::DisplayConnectionType type = display::DISPLAY_CONNECTION_TYPE_NONE;
  bool is_aspect_preserving_scaling = false;
  bool has_overscan = false;
  bool has_color_correction_matrix = false;
  bool color_correction_in_linear_space = false;
  gfx::ColorSpace color_space;
  uint32_t bits_per_channel = 0;
  std::string display_name;
  base::FilePath sys_path;
  std::vector<DisplayMode_Params> modes;
  display::PanelOrientation panel_orientation =
      display::PanelOrientation::kNormal;
  std::vector<uint8_t> edid;
  bool has_current_mode = false;
  DisplayMode_Params current_mode;
  bool has_native_mode = false;
  DisplayMode_Params native_mode;
  int64_t product_code = 0;
  int32_t year_of_manufacture = display::kInvalidYearOfManufacture;
  gfx::Size maximum_cursor_size;
};

struct OverlayCheck_Params {
  OverlayCheck_Params();
  OverlayCheck_Params(const OverlaySurfaceCandidate& candidate);
  OverlayCheck_Params(const OverlayCheck_Params& other);
  ~OverlayCheck_Params();

  gfx::Size buffer_size;
  gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::BufferFormat format = gfx::BufferFormat::BGRA_8888;
  gfx::Rect display_rect;
  gfx::RectF crop_rect;
  bool is_opaque = false;
  int plane_z_order = 0;
  // By default we mark this configuration valid for promoting it to an overlay.
  bool is_overlay_candidate = true;
};

struct OverlayCheckReturn_Params {
  OverlayCheckReturn_Params() = default;
  OverlayCheckReturn_Params(const OverlayCheckReturn_Params& other) = default;
  ~OverlayCheckReturn_Params() = default;

  OverlayStatus status = OVERLAY_STATUS_PENDING;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GPU_OZONE_GPU_MESSAGE_PARAMS_H_
