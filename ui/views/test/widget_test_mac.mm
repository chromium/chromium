// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

namespace {

// The NSWindow last activated by SimulateNativeActivate(). It will have a
// simulated deactivate on a subsequent call.
NSWindow* g_simulated_active_window_ = nil;

}  // namespace

// static
void WidgetTest::SimulateNativeActivate(Widget* widget) {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  if (g_simulated_active_window_) {
    [center postNotificationName:NSWindowDidResignKeyNotification
                          object:g_simulated_active_window_];
  }

  g_simulated_active_window_ = widget->GetNativeWindow().GetNativeNSWindow();
  DCHECK(g_simulated_active_window_);

  // For now, don't simulate main status or windows that can't activate.
  DCHECK([g_simulated_active_window_ canBecomeKeyWindow]);
  [center postNotificationName:NSWindowDidBecomeKeyNotification
                        object:g_simulated_active_window_];
}

// static
bool WidgetTest::IsNativeWindowVisible(gfx::NativeWindow window) {
  return [window.GetNativeNSWindow() isVisible];
}

// static
bool WidgetTest::IsWindowStackedAbove(Widget* above, Widget* below) {
  // Since 10.13, a trip to the runloop has been necessary to ensure [NSApp
  // orderedWindows] has been updated.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(above->IsVisible());
  EXPECT_TRUE(below->IsVisible());

  // -[NSApplication orderedWindows] are ordered front-to-back.
  NSWindow* first = above->GetNativeWindow().GetNativeNSWindow();
  NSWindow* second = below->GetNativeWindow().GetNativeNSWindow();

  for (NSWindow* window in [NSApp orderedWindows]) {
    if (window == second)
      return !first;

    if (window == first)
      first = nil;
  }
  return false;
}

gfx::Size WidgetTest::GetNativeWidgetMinimumContentSize(Widget* widget) {
  return gfx::Size(
      [widget->GetNativeWindow().GetNativeNSWindow() contentMinSize]);
}

// static
ui::EventSink* WidgetTest::GetEventSink(Widget* widget) {
  return static_cast<internal::RootView*>(widget->GetRootView());
}

// static
ui::internal::InputMethodDelegate* WidgetTest::GetInputMethodDelegateForWidget(
    Widget* widget) {
  return NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      widget->GetNativeWindow());
}

// static
bool WidgetTest::IsNativeWindowTransparent(gfx::NativeWindow window) {
  return ![window.GetNativeNSWindow() isOpaque];
}

// static
bool WidgetTest::WidgetHasInProcessShadow(Widget* widget) {
  return false;
}

// static
Widget::Widgets WidgetTest::GetAllWidgets() {
  Widget::Widgets all_widgets;
  for (NSWindow* window : [NSApp windows]) {
    if (Widget* widget = Widget::GetWidgetForNativeWindow(window))
      all_widgets.insert(widget);
  }
  return all_widgets;
}

// static
void WidgetTest::WaitForSystemAppActivation() {
  if (base::mac::IsAtMostOS10_14())
    return;

  // This seems to be only necessary on 10.15+ but it's obscure why. Shortly
  // after launching an app, the system sends ApplicationDidFinishLaunching
  // (which is normal), which causes AppKit on 10.15 to try to find a window to
  // activate. If it finds one it will makeKeyAndOrderFront: it, which breaks
  // tests that are deliberately creating inactive windows.
  base::scoped_nsobject<WindowedNSNotificationObserver> observer(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSApplicationDidFinishLaunchingNotification
                       object:NSApp]);
  [observer wait];
}

}  // namespace test
}  // namespace views
