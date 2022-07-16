// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "ui/base/glib/glib_cast.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/events/event_utils.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

GtkUiPlatformWayland::GtkUiPlatformWayland() {
  gdk_set_allowed_backends("wayland");
  // GDK_BACKEND takes precedence over gdk_set_allowed_backends(), so override
  // it to ensure we get the wayland backend.
  base::Environment::Create()->SetVar("GDK_BACKEND", "wayland");
}

GtkUiPlatformWayland::~GtkUiPlatformWayland() = default;

void GtkUiPlatformWayland::OnInitialized(GtkWidget* widget) {
  // Nothing to do upon initialization for Wayland.
}

GdkKeymap* GtkUiPlatformWayland::GetGdkKeymap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

GdkModifierType GtkUiPlatformWayland::GetGdkKeyEventState(
    const ui::KeyEvent& key_event) {
  const ui::Event::Properties* properties = key_event.properties();
  if (!properties)
    return static_cast<GdkModifierType>(0);
  auto it = properties->find(ui::kPropertyKeyboardState);
  if (it == properties->end())
    return static_cast<GdkModifierType>(0);
  DCHECK_EQ(it->second.size(), 4u);
  // Stored in little endian.
  int flags = 0;
  int bitshift = 0;
  for (uint8_t value : it->second) {
    flags |= value << bitshift;
    bitshift += 8;
  }
  return ExtractGdkEventStateFromKeyEventFlags(flags);
}

int GtkUiPlatformWayland::GetGdkKeyEventGroup(const ui::KeyEvent& key_event) {
  auto state = GetGdkKeyEventState(key_event);
  // See XkbGroupForCoreState() in //ui/events/x/x11_event_translation.cc.
  return (state >> 13) & 0x3;
}

GdkWindow* GtkUiPlatformWayland::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool GtkUiPlatformWayland::ExportWindowHandle(
    gfx::AcceleratedWidget window_id,
    base::OnceCallback<void(std::string)> callback) {
  if (!gtk::GtkCheckVersion(3, 22)) {
    LOG(WARNING) << "set_transient_for_exported not supported in GTK version "
                 << gtk_get_major_version() << '.' << gtk_get_minor_version()
                 << '.' << gtk_get_micro_version();
    return false;
  }

  return ui::LinuxUiDelegate::GetInstance()->ExportWindowHandle(
      window_id,
      base::BindOnce(&GtkUiPlatformWayland::OnHandleForward,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool GtkUiPlatformWayland::SetGtkWidgetTransientFor(
    GtkWidget* widget,
    gfx::AcceleratedWidget parent) {
  return ui::LinuxUiDelegate::GetInstance()->ExportWindowHandle(
      parent, base::BindOnce(&GtkUiPlatformWayland::OnHandleSetTransient,
                             weak_factory_.GetWeakPtr(), widget));
}

void GtkUiPlatformWayland::ClearTransientFor(gfx::AcceleratedWidget parent) {
  // Nothing to do here.
}

void GtkUiPlatformWayland::ShowGtkWindow(GtkWindow* window) {
  // TODO(crbug.com/1008755): Check if gtk_window_present_with_time is needed
  // here as well, similarly to what is done in X11 impl.
  gtk_window_present(window);
}

void GtkUiPlatformWayland::OnHandleSetTransient(GtkWidget* widget,
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

void GtkUiPlatformWayland::OnHandleForward(
    base::OnceCallback<void(std::string)> callback,
    const std::string& handle) {
  std::move(callback).Run("wayland:" + handle);
}

bool GtkUiPlatformWayland::PreferGtkIme() {
  return gtk::GtkCheckVersion(4);
}

}  // namespace gtk
