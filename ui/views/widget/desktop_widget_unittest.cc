// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

using DesktopScreenPositionClientTest = test::DesktopWidgetTest;

// Verifies setting the bounds of a dialog parented to a Widget with a
// PlatformDesktopNativeWidget is positioned correctly.
TEST_F(DesktopScreenPositionClientTest, PositionDialog) {
  Widget parent_widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(10, 11, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_widget.Init(std::move(params));

  // Owned by |dialog|.
  DialogDelegateView* dialog_delegate_view = new DialogDelegateView;
  // Owned by |parent_widget|.
  Widget* dialog = DialogDelegate::CreateDialogWidget(
      dialog_delegate_view, nullptr, parent_widget.GetNativeView());
  dialog->SetBounds(gfx::Rect(11, 12, 200, 200));
  EXPECT_EQ(gfx::Point(11, 12), dialog->GetWindowBoundsInScreen().origin());
}

// Verifies that setting the bounds of a control parented to something other
// than the root window is positioned correctly.
TEST_F(DesktopScreenPositionClientTest, PositionControlWithNonRootParent) {
  Widget widget1;
  Widget widget2;
  Widget widget3;
  gfx::Point origin = gfx::Point(16, 16);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestPoint(origin).work_area();

  // Use a custom frame type.  By default we will choose a native frame when
  // aero glass is enabled, and this complicates the logic surrounding origin
  // computation, making it difficult to compute the expected origin location.
  widget1.set_frame_type(Widget::FrameType::kForceCustom);
  widget2.set_frame_type(Widget::FrameType::kForceCustom);
  widget3.set_frame_type(Widget::FrameType::kForceCustom);

  // Create 3 windows.  A root window, an arbitrary window parented to the root
  // but NOT positioned at (0,0) relative to the root, and then a third window
  // parented to the second, also not positioned at (0,0).
  Widget::InitParams params1 = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params1.bounds = gfx::Rect(
      origin + work_area.OffsetFromOrigin(),
      gfx::Size(700, work_area.height() - origin.y() - work_area.y()));
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(std::move(params1));

  Widget::InitParams params2 = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params2.bounds = gfx::Rect(origin, gfx::Size(600, work_area.height() - 100));
  params2.parent = widget1.GetNativeView();
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.child = true;
  params2.native_widget = test::CreatePlatformNativeWidgetImpl(
      &widget2, test::kStubCapture, nullptr);
  widget2.Init(std::move(params2));

  Widget::InitParams params3 = CreateParams(Widget::InitParams::TYPE_CONTROL);
  params3.parent = widget2.GetNativeView();
  params3.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params3.child = true;
  params3.bounds = gfx::Rect(origin, gfx::Size(500, work_area.height() - 200));
  params3.native_widget = test::CreatePlatformNativeWidgetImpl(
      &widget3, test::kStubCapture, nullptr);
  widget3.Init(std::move(params3));

  // The origin of the 3rd window should be the sum of all parent origins.
  gfx::Point expected_origin(origin.x() * 3 + work_area.x(),
                             origin.y() * 3 + work_area.y());
  gfx::Rect expected_bounds(expected_origin,
                            gfx::Size(500, work_area.height() - 200));
  gfx::Rect actual_bounds(widget3.GetWindowBoundsInScreen());
  EXPECT_EQ(expected_bounds, actual_bounds);
}

// Verifies that the initial bounds of the widget is fully on the screen.
TEST_F(DesktopScreenPositionClientTest, InitialBoundsConstrainedToDesktop) {
  Widget widget;
  // Use the primary display for this test.
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  // Make the origin start at 75% of the width and height.
  gfx::Point origin =
      gfx::Point(work_area.width() * 3 / 4, work_area.height() * 3 / 4);

  // Use a custom frame type. See above for further explanation.
  widget.set_frame_type(Widget::FrameType::kForceCustom);

  // Create a window that is intentionally positioned so that it is off screen.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(
      origin, gfx::Size(work_area.width() / 2, work_area.height() / 2));
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(params));

  // The bounds of the window should be fully on the primary display.
  gfx::Point expected_origin(work_area.right() - work_area.width() / 2,
                             work_area.bottom() - work_area.height() / 2);
  gfx::Rect expected_bounds(expected_origin, gfx::Size(work_area.width() / 2,
                                                       work_area.height() / 2));
  gfx::Rect actual_bounds(widget.GetWindowBoundsInScreen());
  EXPECT_EQ(expected_bounds, actual_bounds);
}

// Verifies that the initial bounds of the widget is fully within the bounds of
// the parent.
TEST_F(DesktopScreenPositionClientTest, InitialBoundsConstrainedToParent) {
  Widget widget1;
  Widget widget2;
  // Use the primary display for this test.
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Point origin = gfx::Point(work_area.x() + work_area.width() / 4,
                                 work_area.y() + work_area.height() / 4);

  // Use a custom frame type.  See above for further explanation
  widget1.set_frame_type(Widget::FrameType::kForceCustom);
  widget2.set_frame_type(Widget::FrameType::kForceCustom);

  // Create 2 windows.  A root window, and an arbitrary window parented to the
  // root and positioned such that it extends beyond the bounds of the root.
  Widget::InitParams params1 = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params1.bounds = gfx::Rect(
      origin, gfx::Size(work_area.width() / 2, work_area.height() / 2));
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(std::move(params1));

  gfx::Rect widget_bounds(widget1.GetWindowBoundsInScreen());

  Widget::InitParams params2 = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params2.bounds =
      gfx::Rect(widget_bounds.width() * 3 / 4, widget_bounds.height() * 3 / 4,
                widget_bounds.width() / 2, widget_bounds.height() / 2);
  params2.parent = widget1.GetNativeView();
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.child = true;
  params2.native_widget = test::CreatePlatformNativeWidgetImpl(
      &widget2, test::kStubCapture, nullptr);
  widget2.Init(std::move(params2));

  // The bounds of the child window should be fully in the parent.
  gfx::Point expected_origin(
      widget_bounds.right() - widget_bounds.width() / 2,
      widget_bounds.bottom() - widget_bounds.height() / 2);
  gfx::Rect expected_bounds(
      expected_origin,
      gfx::Size(widget_bounds.width() / 2, widget_bounds.height() / 2));
  gfx::Rect actual_bounds(widget2.GetWindowBoundsInScreen());
  EXPECT_EQ(expected_bounds, actual_bounds);
}

}  // namespace views
