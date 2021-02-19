// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_ui_delegate_wayland.h"

#include <gtk/gtk.h>

#include <memory>

#include "base/bind.h"
#include "base/environment.h"
#include "base/logging.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if GTK_CHECK_VERSION(3, 90, 0)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>

#define WEAK_GTK_FN(x) extern "C" __attribute__((weak)) decltype(x) x

WEAK_GTK_FN(gdk_wayland_window_set_transient_for_exported);
#endif

namespace ui {

GtkUiDelegateWayland::GtkUiDelegateWayland(WaylandConnection* connection)
    : connection_(connection) {
  DCHECK(connection_);
  gdk_set_allowed_backends("wayland");
  // GDK_BACKEND takes precedence over gdk_set_allowed_backends(), so override
  // it to ensure we get the wayland backend.
  base::Environment::Create()->SetVar("GDK_BACKEND", "wayland");
}

GtkUiDelegateWayland::~GtkUiDelegateWayland() = default;

void GtkUiDelegateWayland::OnInitialized() {
  // Nothing to do upon initialization for Wayland.
}

GdkKeymap* GtkUiDelegateWayland::GetGdkKeymap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

GdkWindow* GtkUiDelegateWayland::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool GtkUiDelegateWayland::SetGdkWindowTransientFor(
    GdkWindow* window,
    gfx::AcceleratedWidget parent) {
#if !GTK_CHECK_VERSION(3, 90, 0)
  if (!gdk_wayland_window_set_transient_for_exported) {
    LOG(WARNING) << "set_transient_for_exported not supported in GTK version "
                 << GTK_MAJOR_VERSION << '.' << GTK_MINOR_VERSION << '.'
                 << GTK_MICRO_VERSION;
    return false;
  }
#endif

  auto* parent_window =
      connection_->wayland_window_manager()->GetWindow(parent);
  auto* foreign = connection_->xdg_foreign();
  if (!parent_window || !foreign)
    return false;

  DCHECK_EQ(parent_window->type(), PlatformWindowType::kWindow);

  foreign->ExportSurfaceToForeign(
      parent_window, base::BindOnce(&GtkUiDelegateWayland::OnHandle,
                                    weak_factory_.GetWeakPtr(), window));
  return true;
}

void GtkUiDelegateWayland::ClearTransientFor(gfx::AcceleratedWidget parent) {
  // Nothing to do here.
}

void GtkUiDelegateWayland::ShowGtkWindow(GtkWindow* window) {
  // TODO(crbug.com/1008755): Check if gtk_window_present_with_time is needed
  // here as well, similarly to what is done in X11 impl.
  gtk_window_present(window);
}

int GtkUiDelegateWayland::GetGdkKeyState() {
  // TODO(crbug/1159460): Test fcitx unikey IME on ozone/wayland.
  return connection_->event_source()->keyboard_modifiers();
}

void GtkUiDelegateWayland::OnHandle(GdkWindow* window,
                                    const std::string& handle) {
  char* parent = const_cast<char*>(handle.c_str());
#if GTK_CHECK_VERSION(3, 90, 0)
  gdk_wayland_toplevel_set_transient_for_exported(GDK_TOPLEVEL(window), parent);
#else
  gdk_wayland_window_set_transient_for_exported(window, parent);
#endif
}

}  // namespace ui
