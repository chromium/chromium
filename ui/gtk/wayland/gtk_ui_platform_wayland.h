// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_
#define UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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
  void OnInitialized() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  void SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;
  bool IncludeFontScaleInDeviceScale() const override;
  bool IncludeScaleInCursorSize() const override;

 private:
  // Called when xdg-foreign exports a parent window passed in
  // SetGtkWidgetTransientFor.
  void OnHandleSetTransient(GtkWidget* widget, std::string handle);

  base::WeakPtrFactory<GtkUiPlatformWayland> weak_factory_{this};
};

}  // namespace gtk

#endif  // UI_GTK_WAYLAND_GTK_UI_PLATFORM_WAYLAND_H_
