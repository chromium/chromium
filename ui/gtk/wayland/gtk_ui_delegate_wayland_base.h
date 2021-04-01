// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WAYLAND_GTK_UI_DELEGATE_WAYLAND_BASE_H_
#define UI_GTK_WAYLAND_GTK_UI_DELEGATE_WAYLAND_BASE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/gtk/gtk_ui_delegate.h"

namespace ui {

class COMPONENT_EXPORT(GTK_WAYLAND) GtkUiDelegateWaylandBase
    : public GtkUiDelegate {
 public:
  GtkUiDelegateWaylandBase();
  GtkUiDelegateWaylandBase(const GtkUiDelegateWaylandBase&) = delete;
  GtkUiDelegateWaylandBase& operator=(const GtkUiDelegateWaylandBase&) = delete;
  ~GtkUiDelegateWaylandBase() override;

  // GtkUiDelegate:
  void OnInitialized(GtkWidget* widget) override;
  GdkKeymap* GetGdkKeymap() override;
  GdkWindow* GetGdkWindow(gfx::AcceleratedWidget window_id) override;
  bool SetGtkWidgetTransientFor(GtkWidget* widget,
                                gfx::AcceleratedWidget parent) override;
  void ClearTransientFor(gfx::AcceleratedWidget parent) override;
  void ShowGtkWindow(GtkWindow* window) override;

 protected:
  virtual bool SetGtkWidgetTransientForImpl(
      gfx::AcceleratedWidget parent,
      base::OnceCallback<void(const std::string&)> callback) = 0;

 private:
  // Called when xdg-foreign exports a parent window passed in
  // SetGtkWidgetTransientFor.
  void OnHandle(GtkWidget* widget, const std::string& handle);

  base::WeakPtrFactory<GtkUiDelegateWaylandBase> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_GTK_WAYLAND_GTK_UI_DELEGATE_WAYLAND_BASE_H_
