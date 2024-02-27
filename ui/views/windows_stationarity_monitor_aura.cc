// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/windows_stationarity_monitor_aura.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/buildflags.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

namespace views {

WindowsStationarityMonitorAura::WindowsStationarityMonitorAura() {
  aura::Env::GetInstance()->AddObserver(this);
  for (aura::WindowTreeHost* window_tree_host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    OnHostInitialized(window_tree_host);
  }
}

WindowsStationarityMonitorAura::~WindowsStationarityMonitorAura() {
  for (aura::Window* window : tracked_windows_) {
    window->RemoveObserver(this);
  }
  aura::Env::GetInstance()->RemoveObserver(this);
  tracked_windows_.clear();
}

// static
WindowsStationarityMonitorAura* WindowsStationarityMonitorAura::GetInstance() {
  static base::NoDestructor<WindowsStationarityMonitorAura> instance;
  return instance.get();
}

void WindowsStationarityMonitorAura::OnHostInitialized(
    aura::WindowTreeHost* host) {
  auto* window = host->window();
  CHECK(window);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  auto* native_widget = DesktopNativeWidgetAura::ForWindow(window);
  // Non client widget, such as tooltip widget, might be destroyed right before
  // handling mouse events on other windows, which results in those events being
  // ignored.
  if (!native_widget ||
      !Widget::RequiresNonClientView(native_widget->widget_type())) {
    return;
  }
#endif

  tracked_windows_.push_back(window);
  window->AddObserver(this);
}

void WindowsStationarityMonitorAura::OnWindowDestroying(aura::Window* window) {
  std::erase(tracked_windows_, window);
  NotifyWindowStationaryStateChanged();
}

void WindowsStationarityMonitorAura::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  // We will consider from-animation reason the same as not-from-animation
  // reason. The main consumer |InputEventActivationProtector| will block input
  // event even when the window bounds change due to animation.
  NotifyWindowStationaryStateChanged();
}

// static
WindowsStationarityMonitor* WindowsStationarityMonitor::GetInstance() {
  return WindowsStationarityMonitorAura::GetInstance();
}

}  // namespace views
