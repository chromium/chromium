// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_ui_platform_stub.h"

#include "base/functional/callback.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace gtk {

GtkUiPlatformStub::GtkUiPlatformStub() = default;

GtkUiPlatformStub::~GtkUiPlatformStub() = default;

void GtkUiPlatformStub::OnInitialized() {}

GdkWindow* GtkUiPlatformStub::GetGdkWindow(gfx::AcceleratedWidget window_id) {
  return nullptr;
}

void GtkUiPlatformStub::SetGtkWidgetTransientFor(
    GtkWidget* widget,
    gfx::AcceleratedWidget parent) {}

void GtkUiPlatformStub::ClearTransientFor(gfx::AcceleratedWidget parent) {}

void GtkUiPlatformStub::ShowGtkWindow(GtkWindow* window) {
  gtk_window_present(window);
}

std::unique_ptr<ui::LinuxInputMethodContext>
GtkUiPlatformStub::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return nullptr;
}

bool GtkUiPlatformStub::IncludeFontScaleInDeviceScale() const {
  return false;
}

bool GtkUiPlatformStub::IncludeScaleInCursorSize() const {
  return false;
}

}  // namespace gtk
