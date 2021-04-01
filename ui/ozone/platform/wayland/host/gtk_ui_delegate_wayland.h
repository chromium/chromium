// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_

#include "ui/gtk/wayland/gtk_ui_delegate_wayland_base.h"

namespace ui {

class WaylandConnection;

class GtkUiDelegateWayland : public GtkUiDelegateWaylandBase {
 public:
  explicit GtkUiDelegateWayland(WaylandConnection* connection);
  ~GtkUiDelegateWayland() override;

  // GtkUiDelegateWaylandBase:
  bool SetGtkWidgetTransientForImpl(
      gfx::AcceleratedWidget parent,
      base::OnceCallback<void(const std::string&)> callback) override;

  // GtkUiDelegate:
  int GetGdkKeyState() override;

 private:
  WaylandConnection* const connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_
