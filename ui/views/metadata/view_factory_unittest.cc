// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/view_factory.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

using ViewFactoryTest = views::test::WidgetTest;

TEST_F(ViewFactoryTest, TestViewBuilder) {
  views::View* parent = nullptr;
  views::LabelButton* button = nullptr;
  views::LabelButton* scroll_button = nullptr;
  views::ScrollView* scroll_view = nullptr;
  auto view =
      views::Builder<views::View>()
          .CopyAddressTo(&parent)
          .SetEnabled(false)
          .SetVisible(true)
          .SetBackground(views::CreateSolidBackground(SK_ColorWHITE))
          .SetBorder(views::CreateEmptyBorder(gfx::Insets()))
          .AddChildren(
              {views::Builder<views::View>()
                   .SetEnabled(false)
                   .SetVisible(true)
                   .SetProperty(views::kMarginsKey, new gfx::Insets(5)),
               views::Builder<views::View>()
                   .SetGroup(5)
                   .SetID(1)
                   .SetFocusBehavior(views::View::FocusBehavior::NEVER),
               views::Builder<views::LabelButton>()
                   .CopyAddressTo(&button)
                   .SetIsDefault(true)
                   .SetEnabled(true)
                   .SetText(base::ASCIIToUTF16("Test")),
               views::Builder<views::ScrollView>()
                   .CopyAddressTo(&scroll_view)
                   .SetContents(views::Builder<views::LabelButton>()
                                    .CopyAddressTo(&scroll_button)
                                    .SetText(base::ASCIIToUTF16("ScrollTest")))
                   .SetHeader(views::Builder<views::View>().SetID(2))})
          .Build();
  ASSERT_TRUE(view.get());
  EXPECT_NE(parent, nullptr);
  EXPECT_NE(button, nullptr);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->GetEnabled());
  ASSERT_GT(view->children().size(), size_t{2});
  EXPECT_EQ(view->children()[1]->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);
  EXPECT_EQ(view->children()[2], button);
  EXPECT_EQ(button->GetText(), base::ASCIIToUTF16("Test"));
  EXPECT_NE(scroll_view, nullptr);
  EXPECT_NE(scroll_button, nullptr);
  EXPECT_EQ(scroll_button->GetText(), base::ASCIIToUTF16("ScrollTest"));
  EXPECT_EQ(scroll_button, scroll_view->contents());
}
