// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_X_GTK_UI_DELEGATE_X11_H_
#define UI_GTK_X_GTK_UI_DELEGATE_X11_H_

#include "base/component_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gtk/gtk_ui_delegate.h"

using GdkDisplay = struct _GdkDisplay;

namespace ui {

class GtkEventLoopX11;

// GtkUiDelegate implementation for desktop Linux X11 backends.
//
// TODO(crbug.com/1002674): For now, this is used by both Aura (legacy) and
// Ozone X11. Move this into X11 Ozone backend once Linux Chrome migration to
// Ozone is completed.
class COMPONENT_EXPORT(UI_GTK_X) GtkUiDelegateX11 : public GtkUiDelegate {
 public:
  explicit GtkUiDelegateX11(x11::Connection* connection);
  GtkUiDelegateX11(const GtkUiDelegateX11&) = delete;
  GtkUiDelegateX11& operator=(const GtkUiDelegateX11&) = delete;
  ~GtkUiDelegateX11() override;

  // GtkUiDelegate:
  void OnInitialized(GtkWidget* widget) override;
  GdkKeymap* GetGdkKeymap() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;
  int GetGdkKeyState() override;

 private:
  GdkDisplay* GetGdkDisplay();

  x11::Connection* const connection_;
  GdkDisplay* display_ = nullptr;
  std::unique_ptr<GtkEventLoopX11> event_loop_;
};

}  // namespace ui

#endif  // UI_GTK_X_GTK_UI_DELEGATE_X11_H_
