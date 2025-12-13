// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

TEST(IOSurface, OddSizeMultiPlanar) {
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      CreateIOSurface(gfx::Size(101, 99), viz::MultiPlaneFormat::kNV12);
  DCHECK(io_surface);
  // Plane sizes are rounded up.
  // https://crbug.com/1226056
  EXPECT_EQ(IOSurfaceGetWidthOfPlane(io_surface.get(), 1), 51u);
  EXPECT_EQ(IOSurfaceGetHeightOfPlane(io_surface.get(), 1), 50u);
}

}  // namespace

}  // namespace gfx
