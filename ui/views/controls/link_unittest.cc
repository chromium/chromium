// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/base_control_test_widget.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class LinkTest : public test::BaseControlTestWidget {
 public:
  LinkTest() = default;
  LinkTest(const LinkTest&) = delete;
  LinkTest& operator=(const LinkTest&) = delete;
  ~LinkTest() override = default;

  void SetUp() override {
    test::BaseControlTestWidget::SetUp();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget()->GetNativeWindow());
    event_generator_->set_assume_window_at_origin(false);
  }

 protected:
  void CreateWidgetContent(View* container) override {
    // Create a widget containing a link which does not take the full size.
    link_ = container->AddChildView(
        std::make_unique<Link>(base::ASCIIToUTF16("TestLink")));
    link_->SetBoundsRect(
        gfx::ScaleToEnclosedRect(container->GetLocalBounds(), 0.5f));
  }

  Link* link() { return link_; }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

 public:
  Link* link_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

}  // namespace

TEST_F(LinkTest, Metadata) {
  link()->SetMultiLine(true);
  test::TestViewMetadata(link());
}

TEST_F(LinkTest, TestLinkClick) {
  bool link_clicked = false;
  link()->SetCallback(base::BindRepeating(
      [](bool* link_clicked) { *link_clicked = true; }, &link_clicked));
  link()->SizeToPreferredSize();
  gfx::Point point = link()->bounds().CenterPoint();
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  link()->OnMouseReleased(release);
  EXPECT_TRUE(link_clicked);
}

TEST_F(LinkTest, TestLinkTap) {
  bool link_clicked = false;
  link()->SetCallback(base::BindRepeating(
      [](bool* link_clicked) { *link_clicked = true; }, &link_clicked));
  link()->SizeToPreferredSize();
  gfx::Point point = link()->bounds().CenterPoint();
  ui::GestureEvent tap_event(point.x(), point.y(), 0, ui::EventTimeForNow(),
                             ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  link()->OnGestureEvent(&tap_event);
  EXPECT_TRUE(link_clicked);
}

// This test doesn't work on Mac due to crbug.com/1071633.
#if !defined(OS_MAC)
// Tests that hovering and unhovering a link adds and removes an underline.
TEST_F(LinkTest, TestUnderlineOnHover) {
  // A non-hovered link should not be underlined.
  const gfx::Rect link_bounds = link()->GetBoundsInScreen();
  const gfx::Point off_link = link_bounds.bottom_right() + gfx::Vector2d(1, 1);
  event_generator()->MoveMouseTo(off_link);
  EXPECT_FALSE(link()->IsMouseHovered());
  const auto link_underlined = [link = link()]() {
    return !!(link->font_list().GetFontStyle() & gfx::Font::UNDERLINE);
  };
  EXPECT_FALSE(link_underlined());

  // Hovering the link should underline it.
  event_generator()->MoveMouseTo(link_bounds.CenterPoint());
  EXPECT_TRUE(link()->IsMouseHovered());
  EXPECT_TRUE(link_underlined());

  // Un-hovering the link should remove the underline again.
  event_generator()->MoveMouseTo(off_link);
  EXPECT_FALSE(link()->IsMouseHovered());
  EXPECT_FALSE(link_underlined());
}
#endif  // !defined(OS_MAC)

}  // namespace views
