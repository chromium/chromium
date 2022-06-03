// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

constexpr gfx::AcceleratedWidget kPrimaryWidget = 1;

OverlaySurfaceCandidate CreateCandidate(const gfx::Rect& rect,
                                        int plane_z_order) {
  ui::OverlaySurfaceCandidate candidate;
  candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;
  candidate.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  candidate.plane_z_order = plane_z_order;
  candidate.buffer_size = rect.size();
  candidate.display_rect = gfx::RectF(rect);
  candidate.crop_rect = gfx::RectF(rect);
  return candidate;
}

}  // namespace

TEST(WaylandOverlayManagerTest, MultipleOverlayCandidates) {
  WaylandOverlayManager manager;

  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(10, 10, 20, 20), -2),
      CreateCandidate(gfx::Rect(30, 30, 10, 10), -1),
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(40, 40, 20, 20), 1),
      CreateCandidate(gfx::Rect(60, 60, 20, 20), 2)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay. All candidates should be marked as handled.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_TRUE(candidates[2].overlay_handled);
  EXPECT_TRUE(candidates[3].overlay_handled);
  EXPECT_TRUE(candidates[4].overlay_handled);
}

TEST(WaylandOverlayManagerTest, NonIntegerDisplayRect) {
  WaylandOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);

  candidates[0].overlay_handled = false;
  candidates[1].overlay_handled = false;

  // Modify the display_rect for the second candidate so it's non-integer. We
  // will never try to promote this to an overlay. This verifies we don't try to
  // convert the non-integer RectF into a Rect which DCHECKs.
  candidates[1].display_rect = gfx::RectF(9.4, 10.43, 20.11, 20.99);
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
}

}  // namespace ui
