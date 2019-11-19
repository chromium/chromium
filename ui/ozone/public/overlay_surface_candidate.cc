// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/overlay_surface_candidate.h"

#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

OverlaySurfaceCandidate::OverlaySurfaceCandidate() = default;

OverlaySurfaceCandidate::OverlaySurfaceCandidate(
    const OverlaySurfaceCandidate& other) = default;

OverlaySurfaceCandidate::~OverlaySurfaceCandidate() = default;

OverlaySurfaceCandidate& OverlaySurfaceCandidate::operator=(
    const OverlaySurfaceCandidate& other) = default;

bool OverlaySurfaceCandidate::operator<(
    const OverlaySurfaceCandidate& param) const {
  int lwidth = buffer_size.width();
  int lheight = buffer_size.height();
  int rwidth = param.buffer_size.width();
  int rheight = param.buffer_size.height();
  gfx::Rect lrect = gfx::ToNearestRect(display_rect);
  gfx::Rect rrect = gfx::ToNearestRect(param.display_rect);

  return std::tie(plane_z_order, format, lrect, lwidth, lheight, transform,
                  crop_rect, is_opaque) <
         std::tie(param.plane_z_order, param.format, rrect, rwidth, rheight,
                  param.transform, param.crop_rect, param.is_opaque);
}

}  // namespace ui
