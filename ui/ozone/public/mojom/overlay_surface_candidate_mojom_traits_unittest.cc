// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/mojom/overlay_surface_candidate_mojom_traits.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/mojom/overlay_surface_candidate.mojom.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

TEST(OverlaySurfaceCandidateStructTraitsTest, FieldsEqual) {
  ui::OverlaySurfaceCandidate input;

  input.transform = gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
  input.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  input.buffer_size = gfx::Size(6, 7);
  input.display_rect = gfx::RectF(1., 2., 3., 4.);
  input.crop_rect = gfx::RectF(10., 20., 30., 40.);
  input.clip_rect = gfx::Rect(11, 21, 31, 41);
  input.is_clipped = true;
  input.is_opaque = true;
  input.plane_z_order = 42;
  input.overlay_handled = true;

  ui::OverlaySurfaceCandidate output;

  bool success = ui::ozone::mojom::OverlaySurfaceCandidate::Deserialize(
      ui::ozone::mojom::OverlaySurfaceCandidate::Serialize(&input), &output);

  EXPECT_TRUE(success);

  EXPECT_EQ(input.transform, output.transform);
  EXPECT_EQ(input.format, output.format);
  EXPECT_EQ(input.buffer_size, output.buffer_size);
  EXPECT_EQ(input.display_rect, output.display_rect);
  EXPECT_EQ(input.crop_rect, output.crop_rect);
  EXPECT_EQ(input.clip_rect, output.clip_rect);
  EXPECT_EQ(input.is_clipped, output.is_clipped);
  EXPECT_EQ(input.is_opaque, output.is_opaque);
  EXPECT_EQ(input.plane_z_order, output.plane_z_order);
  EXPECT_EQ(input.overlay_handled, output.overlay_handled);
}

TEST(OverlaySurfaceCandidateStructTraitsTest, FalseBools) {
  ui::OverlaySurfaceCandidate input;

  input.is_clipped = false;
  input.is_opaque = false;
  input.overlay_handled = false;

  ui::OverlaySurfaceCandidate output;

  bool success = ui::ozone::mojom::OverlaySurfaceCandidate::Deserialize(
      ui::ozone::mojom::OverlaySurfaceCandidate::Serialize(&input), &output);

  EXPECT_TRUE(success);
  EXPECT_EQ(input.is_clipped, output.is_clipped);
  EXPECT_EQ(input.is_opaque, output.is_opaque);
  EXPECT_EQ(input.overlay_handled, output.overlay_handled);
}

TEST(OverlaySurfaceCandidateStructTraitsTest, OverlayStatus) {
  using OverlayStatusTraits =
      mojo::EnumTraits<ui::ozone::mojom::OverlayStatus, ui::OverlayStatus>;

  std::vector<OverlayStatus> tests = {OVERLAY_STATUS_PENDING,
                                      OVERLAY_STATUS_ABLE, OVERLAY_STATUS_NOT};

  for (const OverlayStatus& input : tests) {
    ui::OverlayStatus output;
    bool success = OverlayStatusTraits::FromMojom(
        OverlayStatusTraits::ToMojom(input), &output);

    EXPECT_TRUE(success);
    EXPECT_EQ(input, output);
  }
}

}  // namespace ui
