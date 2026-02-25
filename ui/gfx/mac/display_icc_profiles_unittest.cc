// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/display_icc_profiles.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gfx {

class DisplayICCProfilesTest : public testing::Test {};

TEST_F(DisplayICCProfilesTest, Basic) {
  DisplayICCProfiles* profiles = DisplayICCProfiles::GetInstance();
  ColorSpace srgb = ColorSpace::CreateSRGB();

  base::apple::ScopedCFTypeRef<CFDataRef> data =
      profiles->GetDataForColorSpace(srgb);
  // data should not be null because sRGB is always added.
  EXPECT_TRUE(data);
}

}  // namespace gfx
