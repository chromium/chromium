// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
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

 protected:
  void CreateWidgetContent(View* container) override {
    link_ = container->AddChildView(
        std::make_unique<Link>(base::ASCIIToUTF16("TestLink")));
  }

  Link* link() { return link_; }

 public:
  Link* link_ = nullptr;
};

}  // namespace

TEST_F(LinkTest, Metadata) {
  link()->SetMultiLine(true);
  test::TestViewMetadata(link());
}

TEST_F(LinkTest, TestLinkClick) {
  bool link_clicked = false;
  link()->set_callback(base::BindRepeating(
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
  link()->set_callback(base::BindRepeating(
      [](bool* link_clicked) { *link_clicked = true; }, &link_clicked));
  link()->SizeToPreferredSize();
  gfx::Point point = link()->bounds().CenterPoint();
  ui::GestureEvent tap_event(point.x(), point.y(), 0, ui::EventTimeForNow(),
                             ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  link()->OnGestureEvent(&tap_event);
  EXPECT_TRUE(link_clicked);
}

}  // namespace views
