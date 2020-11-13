// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mac/io_surface_hdr_metadata.h"

namespace gfx {

namespace {

// Check that empty NSBezierPath is returned for empty SkPath.
TEST(IOSurface, HDRMetadata) {
  gfx::HDRMetadata in;
  in.mastering_metadata.primary_r = PointF(1.0, 2.0);
  in.mastering_metadata.primary_g = PointF(4.0, 5.0);
  in.mastering_metadata.primary_b = PointF(7.0, 8.0);
  in.mastering_metadata.white_point = PointF(10.0, 11.0);
  in.mastering_metadata.luminance_max = 13;
  in.mastering_metadata.luminance_min = 14;
  in.max_content_light_level = 15;
  in.max_frame_average_light_level = 16;

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      CreateIOSurface(gfx::Size(100, 100), gfx::BufferFormat::BGRA_8888));

  gfx::HDRMetadata out;
  EXPECT_FALSE(IOSurfaceGetHDRMetadata(io_surface, out));
  IOSurfaceSetHDRMetadata(io_surface, in);
  EXPECT_TRUE(IOSurfaceGetHDRMetadata(io_surface, out));
  EXPECT_EQ(in, out);
}

}  // namespace

}  // namespace gfx
