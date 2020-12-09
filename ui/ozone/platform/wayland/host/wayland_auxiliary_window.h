// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_AUXILIARY_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_AUXILIARY_WINDOW_H_

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

// A WaylandWindow implementation to show tooltips and arrow windows.
class WaylandAuxiliaryWindow : public WaylandWindow {
 public:
  WaylandAuxiliaryWindow(PlatformWindowDelegate* delegate,
                         WaylandConnection* connection);
  WaylandAuxiliaryWindow(const WaylandAuxiliaryWindow&) = delete;
  WaylandAuxiliaryWindow& operator=(const WaylandAuxiliaryWindow&) = delete;
  ~WaylandAuxiliaryWindow() override;

  // PlatformWindow overrides:
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;

 private:
  // WaylandWindow overrides:
  bool OnInitialize(PlatformWindowInitProperties properties) override;

  // Creates (if necessary) and shows a subsurface window.
  void CreateSubsurface();

  // Finds an appropriate parent window.
  WaylandWindow* FindParentWindow();

  wl::Object<wl_subsurface> subsurface_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_AUXILIARY_WINDOW_H_
