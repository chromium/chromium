// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_caption_button.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/window/caption_button_types.h"

namespace {

constexpr SkColor kBackgroundColors[] = {
    SK_ColorBLACK,  SK_ColorDKGRAY,
    SK_ColorGRAY,   SK_ColorLTGRAY,
    SK_ColorWHITE,  SK_ColorRED,
    SK_ColorYELLOW, SK_ColorCYAN,
    SK_ColorBLUE,   SkColorSetRGB(230, 138, 90),
};

}  // namespace

namespace views {

TEST(FrameCaptionButtonTest, ThemedColorContrast) {
  for (SkColor background_color : kBackgroundColors) {
    SkColor button_color = FrameCaptionButton::GetButtonColor(background_color);
    EXPECT_GE(color_utils::GetContrastRatio(button_color, background_color), 3);
  }
}

TEST(FrameCaptionButtonTest, DefaultAccessibilityFocus) {
  FrameCaptionButton button(Button::PressedCallback(),
                            CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON);
  EXPECT_EQ(View::FocusBehavior::ACCESSIBLE_ONLY, button.GetFocusBehavior());
}

TEST(FrameCaptionButtonTest, MetadataTest) {
  FrameCaptionButton button(Button::PressedCallback(),
                            CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON);
  test::TestViewMetadata(&button);
}

}  // namespace views
