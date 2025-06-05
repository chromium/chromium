// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_PLATFORM_STUB_H_
#define UI_GTK_GTK_UI_PLATFORM_STUB_H_

#include "ui/gtk/gtk_ui_platform.h"

namespace gtk {

class GtkUiPlatformStub : public GtkUiPlatform {
 public:
  GtkUiPlatformStub();
  GtkUiPlatformStub(const GtkUiPlatformStub&) = delete;
  GtkUiPlatformStub& operator=(const GtkUiPlatformStub&) = delete;
  ~GtkUiPlatformStub() override;

  // GtkUiPlatform:
  void OnInitialized() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;
  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;
  bool IncludeFontScaleInDeviceScale() const override;
  bool IncludeScaleInCursorSize() const override;
};

}  // namespace gtk

#endif  // UI_GTK_GTK_UI_PLATFORM_STUB_H_
