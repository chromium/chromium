// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/input_method_context_impl_gtk.h"

#include <cstddef>

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/linux/composition_text_util_pango.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_platform.h"
#include "ui/gtk/gtk_util.h"
#include "ui/linux/linux_ui.h"

namespace gtk {

namespace {

GdkEventKey* GdkEventToKey(GdkEvent* event) {
  DCHECK(!GtkCheckVersion(4));
  auto* key = reinterpret_cast<GdkEventKey*>(event);
  DCHECK(key->type == GdkKeyPress() || key->type == GdkKeyRelease());
  return key;
}

// Get IME KeyEvent's target window. Assumes root aura::Window is set to
// Event::target(), otherwise returns null.
GdkWindow* GetTargetWindow(const ui::KeyEvent& key_event) {
  if (!key_event.target())
    return nullptr;

  aura::Window* window = static_cast<aura::Window*>(key_event.target());
  DCHECK(window) << "KeyEvent target window not set.";

  auto window_id = window->GetHost()->GetAcceleratedWidget();
  return GtkUi::GetPlatform()->GetGdkWindow(window_id);
}

// Translate IME ui::KeyEvent to a GdkEventKey.
GdkEvent* GdkEventFromImeKeyEvent(const ui::KeyEvent& key_event) {
  DCHECK(!GtkCheckVersion(4));
  GdkEvent* event = GdkEventFromKeyEvent(key_event);
  if (!event)
    return nullptr;

  GdkWindow* target_window = GetTargetWindow(key_event);
  if (!target_window) {
    gdk_event_free(event);
    return nullptr;
  }
  GdkEventToKey(event)->window = target_window;
  return event;
}

}  // namespace

InputMethodContextImplGtk::InputMethodContextImplGtk(
    ui::LinuxInputMethodContextDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);

  gtk_context_ = gtk_im_multicontext_new();

  static const char kAllowGtkWaylandIm[] = "allow-gtk-wayland-im";
  static const gchar* const kContextIdWayland = "wayland";
  static const gchar* kContextIdIbus = "ibus";
  const gchar* context_id = gtk_im_multicontext_get_context_id(
      GTK_IM_MULTICONTEXT(gtk_context_.get()));
  // switch to allow wayland IM module if it is picked.
  if (context_id) {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            kAllowGtkWaylandIm) &&
        (std::string(context_id) == kContextIdWayland)) {
      // The wayland IM module doesn't work at all because of our usage of dummy
      // window. So try using ibus module instead. Direct Wayland IM integration
      // is being tracked under crbug.com/40113488.
      // TODO(crbug.com/40801194) Remove this if dummy window is no longer used.
      VLOG(1) << "Overriding wayland IM context to ibus";
      gtk_im_multicontext_set_context_id(
          GTK_IM_MULTICONTEXT(gtk_context_.get()), kContextIdIbus);
    } else {
      // This is the case where a non-wayland IM module is picked as per the
      // user's configuration.
      VLOG(1) << "Using GTK IM context: " << context_id;
    }
  }
  gtk_simple_context_ = gtk_im_context_simple_new();

  auto connect = [&](const char* detailed_signal, auto receiver) {
    for (auto context : {gtk_context_, gtk_simple_context_}) {
      // Unretained() is safe since InputMethodContextImplGtk will own the
      // ScopedGSignal.
      signals_.emplace_back(
          context, detailed_signal,
          base::BindRepeating(receiver, base::Unretained(this)));
    }
  };

  connect("commit", &InputMethodContextImplGtk::OnCommit);
  connect("preedit-changed", &InputMethodContextImplGtk::OnPreeditChanged);
  connect("preedit-end", &InputMethodContextImplGtk::OnPreeditEnd);
  connect("preedit-start", &InputMethodContextImplGtk::OnPreeditStart);
  // TODO(shuchen): Handle operations on surrounding text.
  // "delete-surrounding" and "retrieve-surrounding" signals should be
  // handled.

  if (GtkCheckVersion(4)) {
    gtk_im_context_set_client_widget(gtk_context_, GetDummyWindow());
    gtk_im_context_set_client_widget(gtk_simple_context_, GetDummyWindow());
  }
}

InputMethodContextImplGtk::~InputMethodContextImplGtk() {
  if (gtk_context_) {
    g_object_unref(gtk_context_.ExtractAsDangling());
  }
  if (gtk_simple_context_) {
    g_object_unref(gtk_simple_context_.ExtractAsDangling());
  }
}

// Overridden from ui::LinuxInputMethodContext
bool InputMethodContextImplGtk::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  auto* gtk_context = GetIMContext();
  if (!gtk_context)
    return false;

  GdkEvent* event = nullptr;
  if (!GtkCheckVersion(4)) {
    event = GdkEventFromImeKeyEvent(key_event);
    if (!event) {
      LOG(ERROR) << "Cannot translate a Keyevent to a GdkEvent.";
      return false;
    }

    GdkWindow* target_window = GdkEventToKey(event)->window;
    if (!target_window) {
      LOG(ERROR) << "Cannot get target GdkWindow for KeyEvent.";
      return false;
    }

    SetContextClientWindow(target_window, gtk_context);
  }

  // Convert the last known caret bounds relative to the screen coordinates
  // to a GdkRectangle relative to the client window.
  aura::Window* window = static_cast<aura::Window*>(key_event.target());
  gfx::Rect caret_bounds;
  if (gtk_context == gtk_context_)
    caret_bounds = last_caret_bounds_;
  caret_bounds -= window->GetBoundsInScreen().OffsetFromOrigin();

  // Chrome's DIPs may be different from GTK's DIPs if
  // --force-device-scale-factor is used.
  caret_bounds = ScaleToRoundedRect(
      caret_bounds,
      GetDeviceScaleFactor() / gtk_widget_get_scale_factor(GetDummyWindow()));
  GdkRectangle gdk_rect = {caret_bounds.x(), caret_bounds.y(),
                           caret_bounds.width(), caret_bounds.height()};
  gtk_im_context_set_cursor_location(gtk_context, &gdk_rect);

  if (!GtkCheckVersion(4)) {
    const bool handled =
        GtkImContextFilterKeypress(gtk_context, GdkEventToKey(event));
    gdk_event_free(event);
    return handled;
  }
  // In GTK4, clients can no longer create or modify events.  This makes using
  // the gtk_im_context_filter_keypress() API impossible.  Fortunately, an
  // alternative API called gtk_im_context_filter_key() was added for clients
  // that would have needed to construct their own event.  The parameters to
  // the new API are just a deconstructed version of a KeyEvent.
  bool press = key_event.type() == ui::EventType::kKeyPressed;
  auto* surface =
      gtk_native_get_surface(gtk_widget_get_native(GetDummyWindow()));
  auto* device = gdk_seat_get_keyboard(
      gdk_display_get_default_seat(gdk_display_get_default()));
  auto time = (key_event.time_stamp() - base::TimeTicks()).InMilliseconds();
  auto keycode = GetKeyEventProperty(key_event, ui::kPropertyKeyboardHwKeyCode);
  auto state = GtkUi::GetPlatform()->GetGdkKeyEventState(key_event);
  auto group = GtkUi::GetPlatform()->GetGdkKeyEventGroup(key_event);
  return gtk_im_context_filter_key(gtk_context, press, surface, device, time,
                                   keycode, state, group);
}

bool InputMethodContextImplGtk::IsPeekKeyEvent(const ui::KeyEvent& key_event) {
  // Peek events are only sent to make Lacros work with Wayland. Gtk does not
  // send peek events.
  return false;
}

void InputMethodContextImplGtk::Reset() {
  gtk_im_context_reset(gtk_context_);
  gtk_im_context_reset(gtk_simple_context_);

  // Some input methods may not honour the reset call.
  // Focusing out/in the to make sure it gets reset correctly.
  if (type_ != ui::TEXT_INPUT_TYPE_NONE) {
    gtk_im_context_focus_out(gtk_context_);
    gtk_im_context_focus_in(gtk_context_);
  }
}

void InputMethodContextImplGtk::UpdateFocus(
    bool has_client,
    ui::TextInputType old_type,
    const TextInputClientAttributes& new_client_attributes,
    ui::TextInputClient::FocusReason reason) {
  type_ = new_client_attributes.input_type;

  // We only focus when the focus is in a textfield.
  if (old_type != ui::TEXT_INPUT_TYPE_NONE)
    gtk_im_context_focus_out(gtk_context_);
  if (new_client_attributes.input_type != ui::TEXT_INPUT_TYPE_NONE) {
    gtk_im_context_focus_in(gtk_context_);
  }

  // simple context can be used in any textfield, including password box, and
  // even if the focused text input client's text input type is
  // ui::TEXT_INPUT_TYPE_NONE.
  if (has_client)
    gtk_im_context_focus_in(gtk_simple_context_);
  else
    gtk_im_context_focus_out(gtk_simple_context_);

  if (new_client_attributes.flags & ui::TEXT_INPUT_FLAG_VERTICAL) {
    g_object_set(gtk_context_, "input-hints", GTK_INPUT_HINT_VERTICAL_WRITING,
                 nullptr);
    g_object_set(gtk_simple_context_, "input-hints",
                 GTK_INPUT_HINT_VERTICAL_WRITING, nullptr);
  }
}

void InputMethodContextImplGtk::SetCursorLocation(const gfx::Rect& rect) {
  // Remember the caret bounds so that we can set the cursor location later.
  // gtk_im_context_set_cursor_location() takes the location relative to the
  // client window, which is unknown at this point.  So we'll call
  // gtk_im_context_set_cursor_location() later in DispatchKeyEvent() where
  // (and only where) we know the client window.
  last_caret_bounds_ = rect;
}

void InputMethodContextImplGtk::SetSurroundingText(
    const std::u16string& text,
    const gfx::Range& text_range,
    const gfx::Range& composition_range,
    const gfx::Range& selection_range,
    const std::optional<ui::GrammarFragment>& fragment,
    const std::optional<ui::AutocorrectInfo>& autocorrect) {}

// private:

// GtkIMContext event handlers.

void InputMethodContextImplGtk::OnCommit(GtkIMContext* context, gchar* text) {
  if (context != GetIMContext())
    return;

  delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void InputMethodContextImplGtk::OnPreeditChanged(GtkIMContext* context) {
  if (context != GetIMContext())
    return;

  gchar* str = nullptr;
  PangoAttrList* attrs = nullptr;
  gint cursor_pos = 0;
  gtk_im_context_get_preedit_string(context, &str, &attrs, &cursor_pos);
  ui::CompositionText composition_text;
  ui::ExtractCompositionTextFromGtkPreedit(str, attrs, cursor_pos,
                                           &composition_text);
  g_free(str);
  pango_attr_list_unref(attrs);

  delegate_->OnPreeditChanged(composition_text);
}

void InputMethodContextImplGtk::OnPreeditEnd(GtkIMContext* context) {
  if (context != GetIMContext())
    return;

  delegate_->OnPreeditEnd();
}

void InputMethodContextImplGtk::OnPreeditStart(GtkIMContext* context) {
  if (context != GetIMContext())
    return;

  delegate_->OnPreeditStart();
}

void InputMethodContextImplGtk::SetContextClientWindow(
    GdkWindow* window,
    GtkIMContext* gtk_context) {
  gpointer& gdk_last_set_client_window =
      gtk_context == gtk_simple_context_
          ? gdk_last_set_client_window_for_simple_
          : gdk_last_set_client_window_;

  DCHECK(!GtkCheckVersion(4));
  if (window == gdk_last_set_client_window)
    return;
  gtk_im_context_set_client_window(gtk_context, window);

  // Prevent leaks when overriding last client window
  if (gdk_last_set_client_window)
    g_object_unref(gdk_last_set_client_window);
  gdk_last_set_client_window = window;
}

ui::VirtualKeyboardController*
InputMethodContextImplGtk::GetVirtualKeyboardController() {
  return nullptr;
}

GtkIMContext* InputMethodContextImplGtk::GetIMContext() {
  switch (type_) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return gtk_simple_context_;
    default:
      return gtk_context_;
  }
}

}  // namespace gtk
