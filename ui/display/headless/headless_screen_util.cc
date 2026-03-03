// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/headless/headless_screen_util.h"

#include <algorithm>

#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

using display::Display;
using display::DisplayList;

namespace headless {

namespace {

void OffsetDisplayGeometry(Display& display, int dx, int dy) {
  if (dx == 0 && dy == 0) {
    return;
  }

  gfx::Rect bounds = display.bounds();
  bounds.Offset(dx, dy);
  display.set_bounds(bounds);

  gfx::Rect work_area = display.work_area();
  work_area.Offset(dx, dy);
  display.set_work_area(work_area);
}

}  // namespace

void SetDisplayGeometry(Display& display,
                        const gfx::Rect& bounds_in_pixels,
                        const gfx::Insets& work_area_insets_pixels,
                        float device_pixel_ratio) {
  display.SetScale(device_pixel_ratio);
  display.set_size_in_pixels(bounds_in_pixels.size());
  display.set_native_origin(bounds_in_pixels.origin());

  gfx::SizeF size(bounds_in_pixels.size());
  size.InvScale(display.device_scale_factor());
  gfx::Rect bounds(bounds_in_pixels.origin(), gfx::ToCeiledSize(size));
  display.set_bounds(bounds);

  gfx::Rect work_area = bounds;
  if (!work_area_insets_pixels.IsEmpty()) {
    work_area.Inset(gfx::ScaleToCeiledInsets(
        work_area_insets_pixels, 1.0f / display.device_scale_factor()));
  }
  display.set_work_area(work_area);
}

void UpdateDisplay(Display& display,
                   std::optional<int> left,
                   std::optional<int> top,
                   std::optional<int> width,
                   std::optional<int> height,
                   std::optional<int> top_work_area_inset,
                   std::optional<int> left_work_area_inset,
                   std::optional<int> bottom_work_area_inset,
                   std::optional<int> right_work_area_inset,
                   std::optional<double> device_pixel_ratio,
                   std::optional<int> rotation,
                   std::optional<int> color_depth,
                   std::optional<std::string> label,
                   std::optional<bool> is_internal) {
  // Display maintains its size scaled, so convert it to physical pixels.
  gfx::SizeF size(display.bounds().size());
  size.Scale(display.device_scale_factor());
  gfx::Rect bounds_in_pixels(display.bounds().origin(),
                             gfx::ToCeiledSize(size));
  if (left) {
    bounds_in_pixels.set_x(left.value());
  }
  if (top) {
    bounds_in_pixels.set_y(top.value());
  }
  if (width) {
    bounds_in_pixels.set_width(width.value());
  }
  if (height) {
    bounds_in_pixels.set_height(height.value());
  }

  // Display maintains its work area scaled, so convert it to physical pixels.
  gfx::Insets work_area_insets_pixels;
  if (display.device_scale_factor() == 1.0f) {
    work_area_insets_pixels = display.GetWorkAreaInsets();
  } else {
    gfx::InsetsF insets =
        static_cast<gfx::InsetsF>(display.GetWorkAreaInsets());
    insets.Scale(display.device_scale_factor());
    work_area_insets_pixels = gfx::ToCeiledInsets(insets);
  }

  if (top_work_area_inset) {
    work_area_insets_pixels.set_top(top_work_area_inset.value());
  }
  if (left_work_area_inset) {
    work_area_insets_pixels.set_left(left_work_area_inset.value());
  }
  if (bottom_work_area_inset) {
    work_area_insets_pixels.set_bottom(bottom_work_area_inset.value());
  }
  if (right_work_area_inset) {
    work_area_insets_pixels.set_right(right_work_area_inset.value());
  }

  SetDisplayGeometry(
      display, bounds_in_pixels, work_area_insets_pixels,
      device_pixel_ratio.value_or(display.device_scale_factor()));

  if (rotation) {
    int rotation_degrees = rotation.value();
    CHECK(Display::IsValidRotation(rotation_degrees));
    display.SetRotationAsDegree(rotation_degrees);
  }

  if (color_depth) {
    display.set_color_depth(color_depth.value());
  }

  if (label) {
    display.set_label(label.value());
  }

  if (is_internal &&
      is_internal.value() != display::IsInternalDisplayId(display.id())) {
    if (is_internal.value()) {
      display::AddInternalDisplayId(display.id());
    } else {
      display::RemoveInternalDisplayId(display.id());
    }
  }
}

void SetPrimaryDisplay(DisplayList& display_list, int64_t display_id) {
  auto it = display_list.FindDisplayById(display_id);
  CHECK(it != display_list.displays().end());

  // If there is no primary display, assume it's the one with 0,0 origin.
  auto it_primary = display_list.GetPrimaryDisplayIterator();
  if (it_primary == display_list.displays().end()) {
    it_primary =
        std::ranges::find_if(display_list.displays(), [](const auto& display) {
          return display.bounds().origin().IsOrigin();
        });

    // If there still is no primary display, assume it's the first display. It
    // is guaranteed that we have at last one display because of the requested
    // display id check.
    if (it_primary == display_list.displays().end()) {
      it_primary = display_list.displays().begin();
    }
  }

  // Leave early if the specified display is already primary unless it has non
  // zero origin in which case we still want to offset the coordinate system so
  // that the primary screen has zero origin.
  if (it == it_primary && it_primary->bounds().origin().IsOrigin()) {
    return;
  }

  int dx = -it->bounds().origin().x();
  int dy = -it->bounds().origin().y();

  // Update primary display first.
  Display primary_display = *it;
  OffsetDisplayGeometry(primary_display, dx, dy);
  display_list.UpdateDisplay(primary_display, DisplayList::Type::PRIMARY);

  // Update all the secondary displays.
  for (Display display : display_list.displays()) {
    if (display.id() != primary_display.id()) {
      OffsetDisplayGeometry(display, dx, dy);
      display_list.UpdateDisplay(display, DisplayList::Type::NOT_PRIMARY);
    }
  }
}

}  // namespace headless
