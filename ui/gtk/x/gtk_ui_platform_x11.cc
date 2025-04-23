// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/x/gtk_ui_platform_x11.h"

#include "base/check.h"
#include "base/environment.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/xlib_support.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/gtk/input_method_context_impl_gtk.h"
#include "ui/linux/linux_ui_delegate.h"

namespace gtk {

GtkUiPlatformX11::GtkUiPlatformX11() : connection_(*x11::Connection::Get()) {
  gdk_set_allowed_backends("x11");
  auto env = base::Environment::Create();

  // GDK_BACKEND takes precedence over gdk_set_allowed_backends(), so override
  // it to ensure we get the x11 backend.
  env->SetVar("GDK_BACKEND", "x11");

  // Ensure IMEs are running in synchronous mode. Other IMEs (uim, scim, iiim)
  // are already synchronous.
  env->SetVar("IBUS_ENABLE_SYNC_MODE", "1");
  env->SetVar("FCITX_ENABLE_SYNC_MODE", "1");

  x11::InitXlib();
}

GtkUiPlatformX11::~GtkUiPlatformX11() = default;

void GtkUiPlatformX11::OnInitialized() {
  // GTK sets an Xlib error handler that exits the process on any async errors.
  // We don't want this behavior, so reset the error handler to something that
  // just logs the error.
  x11::SetXlibErrorHandler();
}

bool GtkUiPlatformX11::SetGtkWidgetTransientFor(GtkWidget* widget,
                                                gfx::AcceleratedWidget parent) {
  auto x11_window = static_cast<x11::Window>(
      gtk::GtkCheckVersion(4)
          ? gdk_x11_surface_get_xid(
                gtk_native_get_surface(gtk_widget_get_native(widget)))
          : gdk_x11_window_get_xid(gtk_widget_get_window(widget)));
  connection_->SetProperty(x11_window, x11::Atom::WM_TRANSIENT_FOR,
                           x11::Atom::WINDOW, parent);
  connection_->SetProperty(x11_window, x11::GetAtom("_NET_WM_WINDOW_TYPE"),
                           x11::Atom::ATOM,
                           x11::GetAtom("_NET_WM_WINDOW_TYPE_DIALOG"));

  ui::LinuxUiDelegate::GetInstance()->SetTransientWindowForParent(
      parent, static_cast<gfx::AcceleratedWidget>(x11_window));
  return true;
}

void GtkUiPlatformX11::ClearTransientFor(gfx::AcceleratedWidget parent) {
  ui::LinuxUiDelegate::GetInstance()->SetTransientWindowForParent(
      parent, static_cast<gfx::AcceleratedWidget>(x11::Window::None));
}

void GtkUiPlatformX11::ShowGtkWindow(GtkWindow* window) {
  // We need to call gtk_window_present after making the widgets visible to make
  // sure window gets correctly raised and gets focus.
  DCHECK(ui::X11EventSource::HasInstance());
  gtk_window_present_with_time(
      window,
      static_cast<uint32_t>(ui::X11EventSource::GetInstance()->GetTimestamp()));
}

std::unique_ptr<ui::LinuxInputMethodContext>
GtkUiPlatformX11::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return std::make_unique<InputMethodContextImplGtk>(delegate);
}

bool GtkUiPlatformX11::IncludeFontScaleInDeviceScale() const {
  return true;
}

bool GtkUiPlatformX11::IncludeScaleInCursorSize() const {
  // GTK4 supports per-monitor scaling in 4.9.2+, so the gtk-cursor-theme-size
  // is not premultiplied by the scale factor.
  return GtkCheckVersion(4, 9, 2);
}

}  // namespace gtk
