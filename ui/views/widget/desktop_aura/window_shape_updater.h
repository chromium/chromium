// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_SHAPE_UPDATER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_SHAPE_UPDATER_H_

#include <memory>

#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"

namespace views {

class DesktopNativeWidgetAura;
class DesktopWindowTreeHostPlatform;

// Class to observe the window bounds changed to update window shape
// for the rounded corner of the browser frame.
class WindowShapeUpdater : public aura::WindowObserver {
 public:
  static WindowShapeUpdater* CreateWindowShapeUpdater(
      DesktopWindowTreeHostPlatform* tree_host,
      DesktopNativeWidgetAura* native_widget_aura);

 private:
  WindowShapeUpdater(DesktopWindowTreeHostPlatform* tree_host,
                     DesktopNativeWidgetAura* native_widget_aura);
  WindowShapeUpdater(const WindowShapeUpdater&) = delete;
  WindowShapeUpdater& operator=(const WindowShapeUpdater&) = delete;
  ~WindowShapeUpdater() override = default;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  void OnWindowDestroying(aura::Window* window) override;

  void UpdateWindowShapeFromWindowMask(aura::Window* window);

  DesktopWindowTreeHostPlatform* tree_host_ = nullptr;
  DesktopNativeWidgetAura* native_widget_aura_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_SHAPE_UPDATER_H_
