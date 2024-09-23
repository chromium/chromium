// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/windows_stationarity_monitor_mac.h"

#import <AppKit/AppKit.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace views {

WindowsStationarityMonitorMac::WindowsStationarityMonitorMac()
    : init_native_widget_subscription_(
          views::NativeWidgetMac::RegisterInitNativeWidgetCallback(
              base::BindRepeating(
                  &WindowsStationarityMonitorMac::OnNativeWidgetAdded,
                  base::Unretained(this)))) {
  for (NSWindow* window : [NSApp windows]) {
    auto* widget = Widget::GetWidgetForNativeWindow(window);
    // Ignore any widgets that have been tracked.
    // For example, if the window is a system created NSToolbarFullScreenWindow
    // GetFromNativeWindow() will later interrogate the original NSWindow,
    // result in a tracked widget.
    if (!widget || base::Contains(tracked_windows_, widget)) {
      continue;
    }

    widget->AddObserver(this);
    tracked_windows_.push_back(widget);
  }
}

WindowsStationarityMonitorMac::~WindowsStationarityMonitorMac() {
  for (auto* widget : tracked_windows_) {
    widget->RemoveObserver(this);
  }
  tracked_windows_.clear();
  init_native_widget_subscription_ = {};
}

// static
WindowsStationarityMonitorMac* WindowsStationarityMonitorMac::GetInstance() {
  static base::NoDestructor<WindowsStationarityMonitorMac> instance;
  return instance.get();
}

void WindowsStationarityMonitorMac::OnWidgetDestroying(Widget* widget) {
  widget->RemoveObserver(this);
  std::erase(tracked_windows_, widget);
  NotifyWindowStationaryStateChanged();
}

void WindowsStationarityMonitorMac::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  NotifyWindowStationaryStateChanged();
}

void WindowsStationarityMonitorMac::OnNativeWidgetAdded(
    NativeWidgetMac* native_widget) {
  auto* widget = native_widget->GetWidget();
  DCHECK(widget);
  tracked_windows_.push_back(widget);
  widget->AddObserver(this);
}

// static
WindowsStationarityMonitor* WindowsStationarityMonitor::GetInstance() {
  return WindowsStationarityMonitorMac::GetInstance();
}

}  // namespace views
