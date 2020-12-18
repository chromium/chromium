// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_ui_delegate.h"

namespace ui {

class WaylandConnection;

class GtkUiDelegateWayland : public GtkUiDelegate {
 public:
  explicit GtkUiDelegateWayland(WaylandConnection* connection);
  GtkUiDelegateWayland(const GtkUiDelegateWayland&) = delete;
  GtkUiDelegateWayland& operator=(const GtkUiDelegateWayland&) = delete;
  ~GtkUiDelegateWayland() override;

  // GtkUiDelegate:
  void OnInitialized() override;
  GdkKeymap* GetGdkKeymap() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool SetGdkWindowTransientFor(GdkWindow* window,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;
  int GetGdkKeyState() override;

 private:
  // Called when xdg-foreign exports a parent window passed in
  // SetGdkWindowTransientFor.
  void OnHandle(GdkWindow* window, const std::string& handle);

  WaylandConnection* const connection_;

  base::WeakPtrFactory<GtkUiDelegateWayland> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_UI_DELEGATE_WAYLAND_H_
