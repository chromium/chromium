// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/display/manager/touch_device_manager.h"
#endif

namespace display {

using DisplayInfoTest = testing::Test;

TEST_F(DisplayInfoTest, CreateFromSpec) {
  ManagedDisplayInfo info =
      ManagedDisplayInfo::CreateFromSpecWithID("200x100", 10);
  EXPECT_EQ(10, info.id());
  EXPECT_EQ("0,0 200x100", info.bounds_in_native().ToString());
  EXPECT_EQ("200x100", info.size_in_pixel().ToString());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ("0,0,0,0", info.overscan_insets_in_dip().ToString());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/o", 10);
  EXPECT_EQ("10,20 300x400", info.bounds_in_native().ToString());
  EXPECT_EQ("288x380", info.size_in_pixel().ToString());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ("5,3,5,3", info.overscan_insets_in_dip().ToString());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/ob", 10);
  EXPECT_EQ("10,20 300x400", info.bounds_in_native().ToString());
  EXPECT_EQ("288x380", info.size_in_pixel().ToString());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ("5,3,5,3", info.overscan_insets_in_dip().ToString());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/or", 10);
  EXPECT_EQ("10,20 300x400", info.bounds_in_native().ToString());
  EXPECT_EQ("380x288", info.size_in_pixel().ToString());
  EXPECT_EQ(Display::ROTATE_90, info.GetActiveRotation());
  // TODO(oshima): This should be rotated too. Fix this.
  EXPECT_EQ("5,3,5,3", info.overscan_insets_in_dip().ToString());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/or@1.5", 10);
  EXPECT_EQ("10,20 300x400", info.bounds_in_native().ToString());
  EXPECT_EQ("380x288", info.size_in_pixel().ToString());
  EXPECT_EQ(Display::ROTATE_90, info.GetActiveRotation());
  EXPECT_EQ("5,3,5,3", info.overscan_insets_in_dip().ToString());
  EXPECT_EQ("10,6,10,6", info.GetOverscanInsetsInPixel().ToString());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/l@1.5", 10);
  EXPECT_EQ("10,20 300x400", info.bounds_in_native().ToString());
  EXPECT_EQ(Display::ROTATE_270, info.GetActiveRotation());
  EXPECT_EQ(1.5f, info.zoom_factor());

  info = ManagedDisplayInfo::CreateFromSpecWithID(
      "200x200#300x200|200x200%59.9|100x100%60|150x100*2|150x150*1.25%30", 10);

  EXPECT_EQ("0,0 200x200", info.bounds_in_native().ToString());
  EXPECT_EQ(5u, info.display_modes().size());
  // Modes are sorted in DIP for external display.
  EXPECT_EQ("150x100", info.display_modes()[0].size().ToString());
  EXPECT_EQ("100x100", info.display_modes()[1].size().ToString());
  EXPECT_EQ("150x150", info.display_modes()[2].size().ToString());
  EXPECT_EQ("200x200", info.display_modes()[3].size().ToString());
  EXPECT_EQ("300x200", info.display_modes()[4].size().ToString());

  EXPECT_EQ(0.0f, info.display_modes()[0].refresh_rate());
  EXPECT_EQ(60.0f, info.display_modes()[1].refresh_rate());
  EXPECT_EQ(30.0f, info.display_modes()[2].refresh_rate());
  EXPECT_EQ(59.9f, info.display_modes()[3].refresh_rate());
  EXPECT_EQ(0.0f, info.display_modes()[4].refresh_rate());

  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());
  EXPECT_EQ(1.25f, info.display_modes()[2].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[3].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[4].device_scale_factor());

  EXPECT_FALSE(info.display_modes()[0].native());
  EXPECT_FALSE(info.display_modes()[1].native());
  EXPECT_FALSE(info.display_modes()[2].native());
  EXPECT_FALSE(info.display_modes()[3].native());
  EXPECT_TRUE(info.display_modes()[4].native());
}

}  // namespace display
