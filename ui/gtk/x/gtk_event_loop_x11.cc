// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gtk/x/gtk_event_loop_x11.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "base/functional/bind.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/event.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

namespace {

x11::KeyButMask BuildXkbStateFromGdkEvent(unsigned int state,
                                          unsigned char group) {
  return static_cast<x11::KeyButMask>(state | ((group & 0x3) << 13));
}

x11::Event ConvertGdkEventToKeyEvent(GdkEvent* gdk_event) {
  if (!gtk::GtkCheckVersion(4)) {
    auto* key = reinterpret_cast<GdkEventKey*>(gdk_event);
    DCHECK(key->type == GdkKeyPress() || key->type == GdkKeyRelease());
    x11::Window window = x11::Window::None;
    if (key->window)
      window = static_cast<x11::Window>(gdk_x11_window_get_xid(key->window));

    x11::KeyEvent key_event{
        .opcode = key->type == GdkKeyPress() ? x11::KeyEvent::Press
                                             : x11::KeyEvent::Release,
        .detail = static_cast<x11::KeyCode>(key->hardware_keycode),
        .time = static_cast<x11::Time>(key->time),
        .root = ui::GetX11RootWindow(),
        .event = window,
        .state = BuildXkbStateFromGdkEvent(key->state, key->group),
        .same_screen = true,
    };
    return x11::Event(!!key->send_event, std::move(key_event));
  }

  GdkKeymapKey* keys = nullptr;
  guint* keyvals = nullptr;
  gint n_entries = 0;
  gdk_display_map_keycode(gdk_display_get_default(),
                          gdk_key_event_get_keycode(gdk_event), &keys, &keyvals,
                          &n_entries);
  guint keyval = gdk_key_event_get_keyval(gdk_event);
  GdkKeymapKey keymap_key{0, 0, 0};
  if (keys) {
    for (gint i = 0; i < n_entries; i++) {
      if (keyvals[i] == keyval) {
        keymap_key = keys[i];
        break;
      }
    }
    g_free(keys);
    g_free(keyvals);
  }

  x11::KeyEvent key_event{
      .opcode = gtk::GdkEventGetEventType(gdk_event) == GdkKeyPress()
                    ? x11::KeyEvent::Press
                    : x11::KeyEvent::Release,
      .detail = static_cast<x11::KeyCode>(keymap_key.keycode),
      .time = static_cast<x11::Time>(gtk::GdkEventGetTime(gdk_event)),
      .root = ui::GetX11RootWindow(),
      .event = static_cast<x11::Window>(
          gdk_x11_surface_get_xid(gdk_event_get_surface(gdk_event))),
      .state = BuildXkbStateFromGdkEvent(
          gdk_event_get_modifier_state(gdk_event), keymap_key.group),
      .same_screen = true,
  };
  return x11::Event(false, std::move(key_event));
}

void ProcessGdkEvent(GdkEvent* gdk_event) {
  // This function translates GdkEventKeys into XKeyEvents and puts them to
  // the X event queue.
  //
  // base::MessagePumpX11 is using the X11 event queue and all key events should
  // be processed there.  However, there are cases(*1) that GdkEventKeys are
  // created instead of XKeyEvents.  In these cases, we have to translate
  // GdkEventKeys to XKeyEvents and puts them to the X event queue so our main
  // event loop can handle those key events.
  //
  // (*1) At least ibus-gtk in async mode creates a copy of user's key event and
  // pushes it back to the GDK event queue.  In this case, there is no
  // corresponding key event in the X event queue.  So we have to handle this
  // case.  ibus-gtk is used through gtk-immodule to support IMEs.

  auto event_type = gtk::GtkCheckVersion(4)
                        ? gtk::GdkEventGetEventType(gdk_event)
                        : *reinterpret_cast<GdkEventType*>(gdk_event);
  if (event_type != GdkKeyPress() && event_type != GdkKeyRelease())
    return;

  // We want to process the gtk event; mapped to an X11 event immediately
  // otherwise if we put it back on the queue we may get items out of order.
  x11::Connection::Get()->DispatchEvent(ConvertGdkEventToKeyEvent(gdk_event));
}

}  // namespace

GtkEventLoopX11::GtkEventLoopX11(GtkWidget* widget) {
  if (gtk::GtkCheckVersion(4)) {
    surface_ = gtk_native_get_surface(gtk_widget_get_native(widget));
    signal_ = ScopedGSignal(
        surface_, "event",
        base::BindRepeating(&GtkEventLoopX11::OnEvent, base::Unretained(this)));
  } else {
    gdk_event_handler_set(DispatchGdkEvent, nullptr, nullptr);
  }
}

GtkEventLoopX11::~GtkEventLoopX11() {
  if (!gtk::GtkCheckVersion(4)) {
    gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
                          nullptr, nullptr);
  }
}

gboolean GtkEventLoopX11::OnEvent(GdkSurface* surface, GdkEvent* gdk_event) {
  DCHECK(gtk::GtkCheckVersion(4));
  ProcessGdkEvent(gdk_event);
  return false;
}

// static
void GtkEventLoopX11::DispatchGdkEvent(GdkEvent* gdk_event, gpointer) {
  DCHECK(!gtk::GtkCheckVersion(4));
  ProcessGdkEvent(gdk_event);
  gtk_main_do_event(gdk_event);
}

}  // namespace gtk
