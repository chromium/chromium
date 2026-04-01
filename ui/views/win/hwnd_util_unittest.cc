// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_util.h"

#include <memory>
#include <utility>

#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

using HWNDUtilTest = test::WidgetTest;

TEST_F(HWNDUtilTest, HWNDNativeViewAccessibleForWidget) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  gfx::NativeViewAccessible accessible =
      HWNDNativeViewAccessibleForWidget(widget.get());
  EXPECT_NE(nullptr, accessible);

  // Repeat call should return the same instance (caching).
  EXPECT_EQ(accessible, HWNDNativeViewAccessibleForWidget(widget.get()));
}

TEST_F(HWNDUtilTest, HWNDNativeViewAccessibleForWidgetNull) {
  EXPECT_EQ(nullptr, HWNDNativeViewAccessibleForWidget(nullptr));
}

TEST_F(HWNDUtilTest, HWNDNativeViewAccessibleForView) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  View* root_view = widget->GetRootView();
  View* child_view = root_view->AddChildView(std::make_unique<View>());

  gfx::NativeViewAccessible accessible =
      HWNDNativeViewAccessibleForView(child_view);
  EXPECT_NE(nullptr, accessible);
  EXPECT_EQ(accessible, HWNDNativeViewAccessibleForWidget(widget.get()));
}

TEST_F(HWNDUtilTest, HWNDNativeViewAccessibleForViewNull) {
  EXPECT_EQ(nullptr, HWNDNativeViewAccessibleForView(nullptr));
}

TEST_F(HWNDUtilTest, HWNDNativeViewAccessibleForViewOrphaned) {
  View orphaned_view;
  EXPECT_EQ(nullptr, HWNDNativeViewAccessibleForView(&orphaned_view));
}

TEST_F(HWNDUtilTest, SharedRootWindow) {
  std::unique_ptr<Widget> top_level =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_CONTROL);
  params.parent = top_level->GetNativeView();
  std::unique_ptr<Widget> child = std::make_unique<Widget>();
  child->Init(std::move(params));

  // Verify they share the same root window.
  ASSERT_EQ(top_level->GetNativeWindow()->GetRootWindow(),
            child->GetNativeWindow()->GetRootWindow());

  gfx::NativeViewAccessible accessible1 =
      HWNDNativeViewAccessibleForWidget(top_level.get());
  gfx::NativeViewAccessible accessible2 =
      HWNDNativeViewAccessibleForWidget(child.get());

  EXPECT_NE(nullptr, accessible1);
  EXPECT_EQ(accessible1, accessible2);

  // Also check views.
  EXPECT_EQ(accessible1,
            HWNDNativeViewAccessibleForView(top_level->GetRootView()));
  EXPECT_EQ(accessible1, HWNDNativeViewAccessibleForView(child->GetRootView()));
}

}  // namespace views
