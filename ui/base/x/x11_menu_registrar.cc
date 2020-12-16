// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_menu_registrar.h"

#include <memory>
#include <string>

#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/scoped_ignore_errors.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace {

// Our global instance. Deleted when our Env() is deleted.
ui::X11MenuRegistrar* g_handler = nullptr;

}  // namespace

namespace ui {

// static
X11MenuRegistrar* X11MenuRegistrar::Get() {
  if (!g_handler)
    g_handler = new X11MenuRegistrar;

  return g_handler;
}

X11MenuRegistrar::X11MenuRegistrar() {
  x11::Connection::Get()->AddEventObserver(this);

  x_root_window_events_ = std::make_unique<x11::XScopedEventSelector>(
      ui::GetX11RootWindow(),
      x11::EventMask::StructureNotify | x11::EventMask::SubstructureNotify);
}

X11MenuRegistrar::~X11MenuRegistrar() {
  x11::Connection::Get()->RemoveEventObserver(this);
}

void X11MenuRegistrar::OnEvent(const x11::Event& xev) {
  if (auto* create = xev.As<x11::CreateNotifyEvent>())
    OnWindowCreatedOrDestroyed(true, create->window);
  else if (auto* destroy = xev.As<x11::DestroyNotifyEvent>())
    OnWindowCreatedOrDestroyed(false, destroy->window);
}

void X11MenuRegistrar::OnWindowCreatedOrDestroyed(bool created,
                                                  x11::Window window) {
  // Menus created by Chrome can be drag and drop targets. Since they are
  // direct children of the screen root window and have override_redirect
  // we cannot use regular _NET_CLIENT_LIST_STACKING property to find them
  // and use a separate cache to keep track of them.
  // TODO(varkha): Implement caching of all top level X windows and their
  // coordinates and stacking order to eliminate repeated calls to the X server
  // during mouse movement, drag and shaping events.
  if (created) {
    // The window might be destroyed if the message pump did not get a chance to
    // run but we can safely ignore the X error.
    x11::ScopedIgnoreErrors ignore_errors(x11::Connection::Get());
    XMenuList::GetInstance()->MaybeRegisterMenu(window);
  } else {
    XMenuList::GetInstance()->MaybeUnregisterMenu(window);
  }
}

}  // namespace ui
