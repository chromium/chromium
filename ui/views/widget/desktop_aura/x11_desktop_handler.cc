// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_menu_list.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace {

// Our global instance. Deleted when our Env() is deleted.
views::X11DesktopHandler* g_handler = nullptr;

}  // namespace

namespace views {

// static
X11DesktopHandler* X11DesktopHandler::get() {
  if (!g_handler)
    g_handler = new X11DesktopHandler;

  return g_handler;
}

// static
X11DesktopHandler* X11DesktopHandler::get_dont_create() {
  return g_handler;
}

X11DesktopHandler::X11DesktopHandler()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)) {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  aura::Env::GetInstance()->AddObserver(this);

  x_root_window_events_ = std::make_unique<ui::XScopedEventSelector>(
      x_root_window_,
      PropertyChangeMask | StructureNotifyMask | SubstructureNotifyMask);
}

X11DesktopHandler::~X11DesktopHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11DesktopHandler::AddObserver(X11DesktopHandlerObserver* observer) {
  observers_.AddObserver(observer);
}

void X11DesktopHandler::RemoveObserver(X11DesktopHandlerObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::string X11DesktopHandler::GetWorkspace() {
  if (workspace_.empty())
    UpdateWorkspace();
  return workspace_;
}

bool X11DesktopHandler::UpdateWorkspace() {
  int desktop;
  if (ui::GetCurrentDesktop(&desktop)) {
    workspace_ = base::NumberToString(desktop);
    return true;
  }
  return false;
}

bool X11DesktopHandler::CanDispatchEvent(const ui::PlatformEvent& event) {
  return event->type == CreateNotify || event->type == DestroyNotify ||
         (event->type == PropertyNotify &&
          event->xproperty.window == x_root_window_);
}

uint32_t X11DesktopHandler::DispatchEvent(const ui::PlatformEvent& event) {
  switch (event->type) {
    case PropertyNotify: {
      if (event->xproperty.atom == gfx::GetAtom("_NET_CURRENT_DESKTOP")) {
        if (UpdateWorkspace()) {
          for (views::X11DesktopHandlerObserver& observer : observers_)
            observer.OnWorkspaceChanged(workspace_);
        }
      }
      break;
    }
    case CreateNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xcreatewindow.window);
      break;
    case DestroyNotify:
      OnWindowCreatedOrDestroyed(event->type, event->xdestroywindow.window);
      break;
    default:
      NOTREACHED();
  }

  return ui::POST_DISPATCH_NONE;
}

void X11DesktopHandler::OnWindowInitialized(aura::Window* window) {
}

void X11DesktopHandler::OnWillDestroyEnv() {
  g_handler = nullptr;
  delete this;
}

void X11DesktopHandler::OnWindowCreatedOrDestroyed(int event_type,
                                                   XID window) {
  // Menus created by Chrome can be drag and drop targets. Since they are
  // direct children of the screen root window and have override_redirect
  // we cannot use regular _NET_CLIENT_LIST_STACKING property to find them
  // and use a separate cache to keep track of them.
  // TODO(varkha): Implement caching of all top level X windows and their
  // coordinates and stacking order to eliminate repeated calls to the X server
  // during mouse movement, drag and shaping events.
  if (event_type == CreateNotify) {
    // The window might be destroyed if the message pump did not get a chance to
    // run but we can safely ignore the X error.
    gfx::X11ErrorTracker error_tracker;
    ui::XMenuList::GetInstance()->MaybeRegisterMenu(window);
  } else {
    ui::XMenuList::GetInstance()->MaybeUnregisterMenu(window);
  }
}

}  // namespace views
