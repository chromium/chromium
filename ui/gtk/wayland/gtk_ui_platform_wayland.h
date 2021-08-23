// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_
#define UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/gtk/gtk_ui_platform.h"

namespace gtk {

class GtkUiPlatformWayland : public GtkUiPlatform {
 public:
  GtkUiPlatformWayland();
  GtkUiPlatformWayland(const GtkUiPlatformWayland&) = delete;
  GtkUiPlatformWayland& operator=(const GtkUiPlatformWayland&) = delete;
  ~GtkUiPlatformWayland() override;

  // GtkUiPlatform:
  void OnInitialized(GtkWidget* widget) override;
  GdkKeymap* GetGdkKeymap() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override;
  bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;

 private:
  // Called when xdg-foreign exports a parent window passed in
  // SetGtkWidgetTransientFor.
  void OnHandleSetTransient(GtkWidget* widget, const std::string& handle);
  void OnHandleForward(base::OnceCallback<void(std::string)> callback,
                       const std::string& handle);

  base::WeakPtrFactory<GtkUiPlatformWayland> weak_factory_{this};
};

}  // namespace gtk

#endif  // UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_
