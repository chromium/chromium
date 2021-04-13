// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/wayland/gtk_ui_delegate_wayland_base.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/logging.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/gtk/gtk_compat.h"

namespace ui {

GtkUiDelegateWaylandBase::GtkUiDelegateWaylandBase() {
  CHECK(gtk::LoadGtk());

  gdk_set_allowed_backends("wayland");
  // GDK_BACKEND takes precedence over gdk_set_allowed_backends(), so override
  // it to ensure we get the wayland backend.
  base::Environment::Create()->SetVar("GDK_BACKEND", "wayland");
}

GtkUiDelegateWaylandBase::~GtkUiDelegateWaylandBase() = default;

void GtkUiDelegateWaylandBase::OnInitialized(GtkWidget* widget) {
  // Nothing to do upon initialization for Wayland.
}

GdkKeymap* GtkUiDelegateWaylandBase::GetGdkKeymap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

GdkWindow* GtkUiDelegateWaylandBase::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool GtkUiDelegateWaylandBase::SetGtkWidgetTransientFor(
    GtkWidget* widget,
    gfx::AcceleratedWidget parent) {
  if (!gtk::GtkCheckVersion(3, 22)) {
    LOG(WARNING) << "set_transient_for_exported not supported in GTK version "
                 << GTK_MAJOR_VERSION << '.' << GTK_MINOR_VERSION << '.'
                 << GTK_MICRO_VERSION;
    return false;
  }

  return SetGtkWidgetTransientForImpl(
      parent, base::BindOnce(&GtkUiDelegateWaylandBase::OnHandle,
                             weak_factory_.GetWeakPtr(), widget));
}

void GtkUiDelegateWaylandBase::ClearTransientFor(
    gfx::AcceleratedWidget parent) {
  // Nothing to do here.
}

void GtkUiDelegateWaylandBase::ShowGtkWindow(GtkWindow* window) {
  // TODO(crbug.com/1008755): Check if gtk_window_present_with_time is needed
  // here as well, similarly to what is done in X11 impl.
  gtk_window_present(window);
}

void GtkUiDelegateWaylandBase::OnHandle(GtkWidget* widget,
                                        const std::string& handle) {
  char* parent = const_cast<char*>(handle.c_str());
  if (gtk::GtkCheckVersion(4)) {
    auto* toplevel = GlibCast<GdkToplevel>(
        gtk_native_get_surface(gtk_widget_get_native(widget)),
        gdk_toplevel_get_type());
    gdk_wayland_toplevel_set_transient_for_exported(toplevel, parent);
  } else {
    gdk_wayland_window_set_transient_for_exported(gtk_widget_get_window(widget),
                                                  parent);
  }
}

}  // namespace ui
