// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/hit_test_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

using GetHitTestComponentTest = ViewsTestBase;

TEST_F(GetHitTestComponentTest, BasicTests) {
  Widget* widget = new Widget;
  widget->Init(CreateParams(Widget::InitParams::TYPE_WINDOW));

  // Testing arrangement diagram:
  // *=============root:HTCLIENT=============*
  // | *=left:HTLEFT=* *=nowhere:HTNOWHERE=* |
  // | |             | | *=right:HTRIGHT=* | |
  // | |             | | |               | | |
  // | |             | | *===============* | |
  // | *=============* *===================* |
  // *=======================================*
  View* root = widget->GetRootView();
  root->SetProperty(views::kHitTestComponentKey, static_cast<int>(HTCLIENT));
  root->SetBounds(0, 0, 100, 100);

  View* left = new View;
  left->SetProperty(views::kHitTestComponentKey, static_cast<int>(HTLEFT));
  left->SetBounds(10, 10, 30, 80);
  root->AddChildView(left);

  View* nowhere = new View;
  nowhere->SetBounds(60, 10, 30, 80);
  root->AddChildView(nowhere);

  View* right = new View;
  right->SetProperty(views::kHitTestComponentKey, static_cast<int>(HTRIGHT));
  right->SetBounds(10, 10, 10, 60);
  nowhere->AddChildView(right);

  // Hit the root view.
  EXPECT_EQ(GetHitTestComponent(root, gfx::Point(50, 50)), HTCLIENT);

  // Hit the left view.
  EXPECT_EQ(GetHitTestComponent(root, gfx::Point(25, 50)), HTLEFT);

  // Hit the nowhere view, should return the root view's value.
  EXPECT_EQ(GetHitTestComponent(root, gfx::Point(65, 50)), HTCLIENT);

  // Hit the right view.
  EXPECT_EQ(GetHitTestComponent(root, gfx::Point(75, 50)), HTRIGHT);

  // Hit outside the root view.
  EXPECT_EQ(GetHitTestComponent(root, gfx::Point(200, 50)), HTNOWHERE);

  widget->CloseNow();
}

}  // namespace views
