// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using ProgressBarTest = ViewsTestBase;

TEST_F(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetValue(0.62);

  ui::AXNodeData node_data;
  bar.GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kProgressIndicator, node_data.role);
  EXPECT_EQ(base::string16(),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kRestriction));
}

// Test that default colors can be overridden. Used by Chromecast.
TEST_F(ProgressBarTest, OverrideDefaultColors) {
  ProgressBar bar;
  EXPECT_NE(SK_ColorRED, bar.GetForegroundColor());
  EXPECT_NE(SK_ColorGREEN, bar.GetBackgroundColor());
  EXPECT_NE(bar.GetForegroundColor(), bar.GetBackgroundColor());

  bar.SetForegroundColor(SK_ColorRED);
  bar.SetBackgroundColor(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorRED, bar.GetForegroundColor());
  EXPECT_EQ(SK_ColorGREEN, bar.GetBackgroundColor());
}

}  // namespace views
