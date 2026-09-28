// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/info_bubble.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views::test {

class InfoBubbleTest : public ViewsTestBase {
 public:
  InfoBubbleTest() = default;
  InfoBubbleTest(const InfoBubbleTest&) = delete;
  InfoBubbleTest& operator=(const InfoBubbleTest&) = delete;
  ~InfoBubbleTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParamsForTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW);
    anchor_widget_ = std::make_unique<Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    ViewsTestBase::TearDown();
  }

  Widget* anchor_widget() { return anchor_widget_.get(); }

 private:
  std::unique_ptr<Widget> anchor_widget_;
};

TEST_F(InfoBubbleTest, CreateInfoBubble) {
  std::u16string text = u"test message";

  InfoBubble* info_bubble = new InfoBubble(anchor_widget()->GetContentsView(),
                                           BubbleBorder::Arrow::TOP_LEFT, text);
  info_bubble->Show();
  TestWidgetObserver bubble_observer(info_bubble->GetWidget());

  EXPECT_EQ(info_bubble->GetAnchorView(), anchor_widget()->GetContentsView());
  EXPECT_EQ(info_bubble->GetAnchorView()->GetWidget(), anchor_widget());
  EXPECT_EQ(text, info_bubble->label_for_testing()->GetText());
  EXPECT_TRUE(info_bubble->GetVisible());
  EXPECT_FALSE(bubble_observer.widget_closed());

  info_bubble->Hide();
  RunPendingMessages();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

// Ensure the InfoBubble is still sized if not supplied with a preferred width.
TEST_F(InfoBubbleTest, TestPreferredWidthNull) {
  InfoBubble* info_bubble =
      new InfoBubble(anchor_widget()->GetContentsView(),
                     BubbleBorder::Arrow::TOP_LEFT, std::u16string());

  auto child = std::make_unique<View>();
  child->SetPreferredSize(gfx::Size(50, 50));

  info_bubble->AddChildView(std::move(child));
  info_bubble->Show();
  EXPECT_LT(0, info_bubble->GetLocalBounds().width());
  info_bubble->Hide();
  RunPendingMessages();
}

TEST_F(InfoBubbleTest, TestPreferredWidth) {
  constexpr int kPreferredWidthLarge = 800;
  constexpr int kPreferredWidthSmall = 50;

  InfoBubble* info_bubble =
      new InfoBubble(anchor_widget()->GetContentsView(),
                     BubbleBorder::Arrow::TOP_LEFT, std::u16string());
  info_bubble->Show();
  info_bubble->set_preferred_width(kPreferredWidthLarge);
  info_bubble->SizeToPreferredSize();

  // Test to make sure the resulting |info_bubble| honors the preferred size.
  // |info_bubble| may be slightly smaller due to having to account for margins
  // and bubble border size.
  EXPECT_GE(kPreferredWidthLarge, info_bubble->GetLocalBounds().width());
  EXPECT_LT(kPreferredWidthSmall, info_bubble->GetLocalBounds().width());

  info_bubble->set_preferred_width(kPreferredWidthSmall);
  info_bubble->SizeToPreferredSize();

  // |info_bubble| should now be at or smaller than the smaller preferred width.
  EXPECT_GE(kPreferredWidthSmall, info_bubble->GetLocalBounds().width());
  info_bubble->Hide();
  RunPendingMessages();
}

TEST_F(InfoBubbleTest, TestInfoBubbleVisibilityHiddenAnchor) {
  anchor_widget()->Hide();

  InfoBubble* info_bubble =
      new InfoBubble(anchor_widget()->GetContentsView(),
                     BubbleBorder::Arrow::TOP_LEFT, std::u16string());
  info_bubble->Show();

  EXPECT_FALSE(info_bubble->GetWidget()->IsVisible());
  info_bubble->Hide();
  RunPendingMessages();
}

TEST_F(InfoBubbleTest, TestInfoBubbleAnchorBoundsChanged) {
  InfoBubble* info_bubble = new InfoBubble(anchor_widget()->GetContentsView(),
                                           BubbleBorder::Arrow::TOP_LEFT, u"");
  info_bubble->Show();

  gfx::Rect original_bounds =
      info_bubble->GetWidget()->GetWindowBoundsInScreen();

  anchor_widget()->SetBounds(original_bounds - gfx::Vector2d(5, 5));

  EXPECT_NE(original_bounds,
            info_bubble->GetWidget()->GetWindowBoundsInScreen());
  info_bubble->Hide();
  RunPendingMessages();
}

// Iterate through the metadata for InfoBubble to ensure it all works.
TEST_F(InfoBubbleTest, MetadataTest) {
  InfoBubble* info_bubble = new InfoBubble(anchor_widget()->GetContentsView(),
                                           BubbleBorder::Arrow::TOP_LEFT, u"");
  info_bubble->Show();

  test::TestViewMetadata(info_bubble);
  info_bubble->Hide();
  RunPendingMessages();
}

TEST_F(InfoBubbleTest, AvailableScreenBoundsUsesWorkArea) {
#if BUILDFLAG(IS_OZONE)
  // On Ozone platforms that do not support global screen coordinates (e.g.
  // Wayland), `BubbleDialogDelegate::GetBubbleBounds()` disables offscreen
  // bubble adjustment (`adjust_to_fix_available_bounds = false`) and delegates
  // positioning to the compositor instead.
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    GTEST_SKIP() << "Global screen coordinates unavailable";
  }
#endif
  const gfx::Rect work_area =
      display::Screen::Get()
          ->GetDisplayNearestWindow(anchor_widget()->GetNativeWindow())
          .work_area();

  // Place `anchor_widget` near the right edge of the display's work area so
  // that it is completely within `work_area`, and place `anchor_view` near the
  // left side of `anchor_widget`. A wide bubble with TOP_LEFT arrow overflows
  // `anchor_widget` on the right less than a mirrored TOP_RIGHT bubble
  // overflows `anchor_widget` on the left, but TOP_LEFT overflows `work_area`
  // whereas TOP_RIGHT fits completely within `work_area`.
  anchor_widget()->SetBounds(
      {work_area.right() - 150, work_area.y() + 100, 140, 200});
  View* anchor_view = anchor_widget()->GetContentsView()->AddChildView(
      std::make_unique<View>());
  anchor_view->SetBoundsRect({10, 10, 20, 20});

  InfoBubble* info_bubble =
      new InfoBubble(anchor_view, BubbleBorder::Arrow::TOP_LEFT, u"");
  info_bubble->set_preferred_width(200);
  info_bubble->Show();
  // Offscreen adjustment is disabled by default on Linux
  // (`PlatformStyle::kAdjustBubbleIfOffscreen`), and `CreateBubble()` resets
  // it to that default. Enable it explicitly so that the bubble is positioned
  // using the available screen bounds.
  info_bubble->set_adjust_if_offscreen(true);
  info_bubble->SizeToContents();

  EXPECT_EQ(
      work_area,
      info_bubble->GetBubbleFrameView()->available_screen_bounds_callback().Run(
          info_bubble->GetAnchorRect()));
  EXPECT_EQ(BubbleBorder::Arrow::TOP_RIGHT,
            info_bubble->GetBubbleFrameView()->GetArrow());
  EXPECT_TRUE(
      work_area.Contains(info_bubble->GetWidget()->GetWindowBoundsInScreen()));

  // When `anchor_widget()` is null, the bubble's widget should be used to find
  // the available work area bounds and position the bubble within `work_area`.
  info_bubble->SetAnchorView(nullptr);
  ASSERT_EQ(nullptr, info_bubble->anchor_widget());
  info_bubble->SetAnchorRect(
      {work_area.right() - 100, work_area.y() + 50, 20, 20});
  EXPECT_EQ(
      work_area,
      info_bubble->GetBubbleFrameView()->available_screen_bounds_callback().Run(
          info_bubble->GetAnchorRect()));
  EXPECT_EQ(BubbleBorder::Arrow::TOP_RIGHT,
            info_bubble->GetBubbleFrameView()->GetArrow());
  EXPECT_TRUE(
      work_area.Contains(info_bubble->GetWidget()->GetWindowBoundsInScreen()));

  info_bubble->Hide();
  RunPendingMessages();
}

}  // namespace views::test
