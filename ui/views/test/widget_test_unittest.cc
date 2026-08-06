// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/mock_activation_controller.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views::test {
namespace {

// Insert |widget| into |expected| and ensure it's reported by GetAllWidgets().
void ExpectAdd(Widget::Widgets* expected, Widget* widget, const char* message) {
  SCOPED_TRACE(message);
  EXPECT_TRUE(expected->insert(widget).second);
  EXPECT_TRUE(std::ranges::equal(*expected, WidgetTest::GetAllWidgets()));
}

// Close |widgets[0]|, and expect all |widgets| to be removed.
void ExpectClose(Widget::Widgets* expected,
                 std::vector<Widget*> widgets,
                 const char* message) {
  SCOPED_TRACE(message);
  for (Widget* widget : widgets) {
    EXPECT_EQ(1u, expected->erase(widget));
  }
  widgets[0]->CloseNow();
  EXPECT_TRUE(std::ranges::equal(*expected, WidgetTest::GetAllWidgets()));
}

}  // namespace

using WidgetTestTest = WidgetTest;

// Ensure that Widgets with various root windows are correctly reported by
// WidgetTest::GetAllWidgets().
TEST_F(WidgetTestTest, GetAllWidgets) {
  // Note Widget::Widgets is a std::set ordered by pointer value, so the order
  // that |expected| is updated below is not important.
  Widget::Widgets expected;

  EXPECT_EQ(expected, GetAllWidgets());

  Widget* platform = CreateTopLevelPlatformWidget();
  ExpectAdd(&expected, platform, "platform");

  Widget* platform_child = CreateChildPlatformWidget(platform->GetNativeView());
  ExpectAdd(&expected, platform_child, "platform_child");

  Widget* frameless = CreateTopLevelFramelessPlatformWidget();
  ExpectAdd(&expected, frameless, "frameless");

  Widget* native = CreateTopLevelNativeWidget();
  ExpectAdd(&expected, native, "native");

  Widget* native_child = CreateChildNativeWidgetWithParent(native);
  ExpectAdd(&expected, native_child, "native_child");

  ExpectClose(&expected, {native, native_child}, "native");
  ExpectClose(&expected, {platform, platform_child}, "platform");
  ExpectClose(&expected, {frameless}, "frameless");
}

using DesktopWidgetTestTest = DesktopWidgetTest;

// As above, but with desktop native widgets (i.e. DesktopNativeWidgetAura on
// Aura).
TEST_F(DesktopWidgetTestTest, GetAllWidgets) {
  // Note Widget::Widgets is a std::set ordered by pointer value, so the order
  // that |expected| is updated below is not important.
  Widget::Widgets expected;

  EXPECT_EQ(expected, GetAllWidgets());

  Widget* frameless = CreateTopLevelFramelessPlatformWidget();
  ExpectAdd(&expected, frameless, "frameless");

  Widget* native = CreateTopLevelNativeWidget();
  ExpectAdd(&expected, native, "native");

  Widget* native_child = CreateChildNativeWidgetWithParent(native);
  ExpectAdd(&expected, native_child, "native_child");

  Widget* desktop = CreateTopLevelNativeWidget();
  ExpectAdd(&expected, desktop, "desktop");

  Widget* desktop_child = CreateChildNativeWidgetWithParent(desktop);
  ExpectAdd(&expected, desktop_child, "desktop_child");

#if defined(USE_AURA)
  // A DesktopWindowTreeHost has both a root aura::Window and a content window.
  // DesktopWindowTreeHostX11::GetAllOpenWindows() returns content windows, so
  // ensure that a Widget parented to the root window is also found.
  Widget* desktop_cousin =
      CreateChildPlatformWidget(desktop->GetNativeView()->GetRootWindow());
  ExpectAdd(&expected, desktop_cousin, "desktop_cousin");
  ExpectClose(&expected, {desktop_cousin}, "desktop_cousin");
#endif  // USE_AURA

  ExpectClose(&expected, {desktop, desktop_child}, "desktop");
  ExpectClose(&expected, {native, native_child}, "native");
  ExpectClose(&expected, {frameless}, "frameless");
}
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
TEST_F(DesktopWidgetTestTest, MockActivationControllerDeactivateDestroy) {
  views::test::MockActivationController controller;
  Widget* widget1 = CreateTopLevelNativeWidget();
  Widget* widget2 = CreateTopLevelNativeWidget();

  widget1->Show();
  controller.MaybeActivate(widget1, true);

  widget2->Show();
  controller.MaybeActivate(widget2, true);

  // Hide widget1 so it is no longer activatable.
  widget1->Hide();

  // Deactivate widget2. Since no other activatable widgets exist, widget2
  // remains in the tracking list, but its activation is cleared.
  controller.Deactivate(widget2);
  EXPECT_TRUE(controller.IsTrackedForTesting(widget2));

  // Destroying widget2 should not trigger an observer leak or check failure
  // because its observer is correctly removed from the tracking list.
  widget2->CloseNow();

  widget1->CloseNow();
}

TEST_F(DesktopWidgetTestTest, MockActivationControllerKeepTrackOfDeactivated) {
  views::test::MockActivationController controller;
  Widget* widget1 = CreateTopLevelNativeWidget();
  Widget* widget2 = CreateTopLevelNativeWidget();

  widget1->Show();
  controller.MaybeActivate(widget1, true);

  widget2->Show();
  controller.MaybeActivate(widget2, true);
  EXPECT_TRUE(controller.IsActive(widget2));
  EXPECT_FALSE(controller.IsActive(widget1));

  // Hide widget1 so it is no longer activatable.
  widget1->Hide();

  // Deactivate widget2. Since no other activatable widgets exist, no widget is
  // active.
  controller.Deactivate(widget2);
  EXPECT_FALSE(controller.IsActive(widget2));
  EXPECT_FALSE(controller.IsActive(widget1));
  EXPECT_TRUE(controller.IsTrackedForTesting(widget2));

  // Show widget1 again and activate it.
  widget1->Show();
  controller.MaybeActivate(widget1, true);
  EXPECT_TRUE(controller.IsActive(widget1));
  EXPECT_FALSE(controller.IsActive(widget2));

  // Deactivate widget1. Since widget2 was kept in the tracking list, it should
  // be found and activated!
  controller.Deactivate(widget1);
  EXPECT_TRUE(controller.IsActive(widget2));
  EXPECT_FALSE(controller.IsActive(widget1));

  widget1->CloseNow();
  widget2->CloseNow();
}
#endif

}  // namespace views::test
