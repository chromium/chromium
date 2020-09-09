// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/x/gtk_ui_delegate_x11.h"

#include <gtk/gtk.h>

#include "base/check.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gtk/x/gtk_event_loop_x11.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

extern "C" {
GdkWindow* gdk_x11_window_foreign_new_for_display(GdkDisplay* display,
                                                  Window window);

GdkWindow* gdk_x11_window_lookup_for_display(GdkDisplay* display,
                                             Window window);

Window gdk_x11_window_get_xid(GdkWindow* window);
}

namespace ui {

GtkUiDelegateX11::GtkUiDelegateX11(x11::Connection* connection)
    : connection_(connection) {
  DCHECK(connection_);
  gdk_set_allowed_backends("x11");
}

GtkUiDelegateX11::~GtkUiDelegateX11() = default;

void GtkUiDelegateX11::OnInitialized() {
  // Ensure the singleton instance of GtkEventLoopX11 is created and started.
  GtkEventLoopX11::EnsureInstance();
}

GdkKeymap* GtkUiDelegateX11::GetGdkKeymap() {
  return gdk_keymap_get_for_display(GetGdkDisplay());
}

GdkWindow* GtkUiDelegateX11::GetGdkWindow(gfx::AcceleratedWidget window_id) {
  GdkDisplay* display = GetGdkDisplay();
  GdkWindow* gdk_window = gdk_x11_window_lookup_for_display(
      display, static_cast<uint32_t>(window_id));
  if (gdk_window)
    g_object_ref(gdk_window);
  else
    gdk_window = gdk_x11_window_foreign_new_for_display(
        display, static_cast<uint32_t>(window_id));
  return gdk_window;
}

bool GtkUiDelegateX11::SetGdkWindowTransientFor(GdkWindow* window,
                                                gfx::AcceleratedWidget parent) {
  auto x11_window = static_cast<x11::Window>(gdk_x11_window_get_xid(window));
  SetProperty(x11_window, x11::Atom::WM_TRANSIENT_FOR, x11::Atom::WINDOW,
              parent);

  ui::X11Window* parent_window =
      ui::X11WindowManager::GetInstance()->GetWindow(parent);
  parent_window->SetTransientWindow(x11_window);

  return true;
}

void GtkUiDelegateX11::ClearTransientFor(gfx::AcceleratedWidget parent) {
  ui::X11Window* parent_window =
      ui::X11WindowManager::GetInstance()->GetWindow(parent);
  // parent_window might be dead if there was a top-down window close
  if (parent_window)
    parent_window->SetTransientWindow(x11::Window::None);
}

GdkDisplay* GtkUiDelegateX11::GetGdkDisplay() {
  if (!display_)
    display_ = gdk_display_get_default();
  return display_;
}

void GtkUiDelegateX11::ShowGtkWindow(GtkWindow* window) {
  // We need to call gtk_window_present after making the widgets visible to make
  // sure window gets correctly raised and gets focus.
  DCHECK(X11EventSource::HasInstance());
  gtk_window_present_with_time(
      window,
      static_cast<uint32_t>(X11EventSource::GetInstance()->GetTimestamp()));
}

}  // namespace ui
