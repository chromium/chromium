// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ui {

// Test to ensure closed caption styling from system settings can be obtained
// (we obtain a CaptionStyle).
TEST(CaptionStyleWinTest, TestWinCaptionStyle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kSystemCaptionStyle);

  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  std::optional<ui::CaptionStyle> caption_style =
      ui::CaptionStyle::FromSystemSettings();
  // On Windows out of the box, all caption style properties are set to
  // Default. In which case, each of these should be empty.
  ASSERT_TRUE(caption_style.has_value());
  EXPECT_TRUE(caption_style->background_color.empty());
  EXPECT_TRUE(caption_style->font_family.empty());
  EXPECT_TRUE(caption_style->font_variant.empty());
  EXPECT_TRUE(caption_style->text_color.empty());
  EXPECT_TRUE(caption_style->text_shadow.empty());
  EXPECT_TRUE(caption_style->text_size.empty());
  EXPECT_TRUE(caption_style->window_color.empty());
}

}  // namespace ui
