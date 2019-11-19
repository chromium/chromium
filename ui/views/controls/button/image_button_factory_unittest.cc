// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button_factory.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using ImageButtonFactoryTest = ViewsTestBase;

TEST_F(ImageButtonFactoryTest, CreateVectorImageButton) {
  auto button = CreateVectorImageButton(nullptr);
  EXPECT_EQ(ImageButton::ALIGN_CENTER, button->h_alignment_);
  EXPECT_EQ(ImageButton::ALIGN_MIDDLE, button->v_alignment_);
}

TEST_F(ImageButtonFactoryTest, SetImageFromVectorIcon) {
  auto button = CreateVectorImageButton(nullptr);
  SetImageFromVectorIcon(button.get(), vector_icons::kCloseRoundedIcon,
                         SK_ColorRED);
  EXPECT_FALSE(button->GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_FALSE(button->GetImage(Button::STATE_DISABLED).isNull());
  EXPECT_EQ(color_utils::DeriveDefaultIconColor(SK_ColorRED),
            button->GetInkDropBaseColor());
}

TEST_F(ImageButtonFactoryTest, SetImageFromVectorIcon_Default) {
  auto button = CreateVectorImageButton(nullptr);
  SetImageFromVectorIcon(button.get(), vector_icons::kCloseRoundedIcon);
  EXPECT_EQ(button->GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_DefaultIconColor),
            button->GetInkDropBaseColor());
}
}  // namespace views
