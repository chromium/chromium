// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/input_method_context_impl_gtk.h"
#include "ui/linux/linux_ui_delegate.h"

namespace gtk {

GtkUiPlatformWayland::GtkUiPlatformWayland() {
  gdk_set_allowed_backends("wayland");
  // GDK_BACKEND takes precedence over gdk_set_allowed_backends(), so override
  // it to ensure we get the wayland backend.
  base::Environment::Create()->SetVar("GDK_BACKEND", "wayland");
}

GtkUiPlatformWayland::~GtkUiPlatformWayland() = default;

void GtkUiPlatformWayland::OnInitialized() {
  // Nothing to do upon initialization for Wayland.
}

GdkWindow* GtkUiPlatformWayland::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

void GtkUiPlatformWayland::SetGtkWidgetTransientFor(
    GtkWidget* widget,
    gfx::AcceleratedWidget parent) {
  ui::LinuxUiDelegate::GetInstance()->ExportWindowHandle(
      parent, base::BindOnce(&GtkUiPlatformWayland::OnHandleSetTransient,
                             weak_factory_.GetWeakPtr(), widget));
}

void GtkUiPlatformWayland::ClearTransientFor(gfx::AcceleratedWidget parent) {
  // Nothing to do here.
}

void GtkUiPlatformWayland::ShowGtkWindow(GtkWindow* window) {
  // TODO(crbug.com/40650162): Check if gtk_window_present_with_time is needed
  // here as well, similarly to what is done in X11 impl.
  gtk_window_present(window);
}

void GtkUiPlatformWayland::OnHandleSetTransient(GtkWidget* widget,
                                                std::string handle) {
  auto handle_no_prefix = base::RemovePrefix(handle, "wayland:");
  if (!handle_no_prefix || handle_no_prefix->empty()) {
    return;
  }
  char* parent = const_cast<char*>(handle_no_prefix->data());
  if (gtk::GtkCheckVersion(4)) {
    auto* toplevel = GlibCast<GdkToplevel>(
        gtk_native_get_surface(gtk_widget_get_native(widget)),
        gdk_toplevel_get_type());
    gdk_wayland_toplevel_set_transient_for_exported(toplevel, parent);
  } else if (gtk::GtkCheckVersion(3, 22)) {
    gdk_wayland_window_set_transient_for_exported(gtk_widget_get_window(widget),
                                                  parent);
  } else {
    LOG(WARNING) << "set_transient_for_exported not supported in GTK version "
                 << gtk_get_major_version() << '.' << gtk_get_minor_version()
                 << '.' << gtk_get_micro_version();
  }
}

std::unique_ptr<ui::LinuxInputMethodContext>
GtkUiPlatformWayland::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  // Use text-input-v3 on Wayland.
  return nullptr;
}

bool GtkUiPlatformWayland::IncludeFontScaleInDeviceScale() const {
  // Assume font scaling will be handled by Ozone/Wayland.
  return true;
}

bool GtkUiPlatformWayland::IncludeScaleInCursorSize() const {
  return false;
}

}  // namespace gtk
