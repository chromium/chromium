// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views::test {
namespace {

// Insert |widget| into |expected| and ensure it's reported by GetAllWidgets().
void ExpectAdd(Widget::Widgets* expected, Widget* widget, const char* message) {
  SCOPED_TRACE(message);
  EXPECT_TRUE(expected->insert(widget).second);
  EXPECT_TRUE(base::ranges::equal(*expected, WidgetTest::GetAllWidgets()));
}

// Close |widgets[0]|, and expect all |widgets| to be removed.
void ExpectClose(Widget::Widgets* expected,
                 std::vector<Widget*> widgets,
                 const char* message) {
  SCOPED_TRACE(message);
  for (Widget* widget : widgets)
    EXPECT_EQ(1u, expected->erase(widget));
  widgets[0]->CloseNow();
  EXPECT_TRUE(base::ranges::equal(*expected, WidgetTest::GetAllWidgets()));
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

}  // namespace views::test
