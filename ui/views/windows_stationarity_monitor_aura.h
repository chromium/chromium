// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_AURA_H_
#define UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_AURA_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/views/windows_stationarity_monitor.h"

namespace aura {
class Window;
class WindowTreeHost;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {

class WindowsStationarityMonitorAura : public WindowsStationarityMonitor,
                                       public aura::EnvObserver,
                                       public aura::WindowObserver {
 public:
  WindowsStationarityMonitorAura();

  WindowsStationarityMonitorAura(const WindowsStationarityMonitorAura&) =
      delete;
  WindowsStationarityMonitorAura& operator=(
      const WindowsStationarityMonitorAura&) = delete;

  static WindowsStationarityMonitorAura* GetInstance();

  // aura::EnvObserver:
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

 private:
  ~WindowsStationarityMonitorAura() override;

  std::vector<raw_ptr<aura::Window, VectorExperimental>> tracked_windows_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_AURA_H_
