// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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

void GtkUiPlatformWayland::OnInitialized(GtkWidget* widget) {
  // Nothing to do upon initialization for Wayland.
}

GdkKeymap* GtkUiPlatformWayland::GetGdkKeymap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

GdkModifierType GtkUiPlatformWayland::GetGdkKeyEventState(
    const ui::KeyEvent& key_event) {
  // We first reconstruct the state that was stored as a property by
  // ui::WaylandEventSource. It is incomplete, however, and includes only the
  // modifier state. Therefore, we compute and add the group manually, if
  // possible.
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
  auto state = ExtractGdkEventStateFromKeyEventFlags(flags);

  // We use the default group 0 in the following three cases:
  //  - we are using GTK 3 (gdk_display_map_keycode() is only available in
  //    GTK 4);
  //  - the pressed/released key is not a character (e.g. a modifier);
  //  - no entry in |keyvals| matches |keyval|.
  unsigned int group = 0;

  if (gtk::GtkCheckVersion(4) && key_event.GetDomKey().IsCharacter()) {
    guint keycode =
        GetKeyEventProperty(key_event, ui::kPropertyKeyboardHwKeyCode);
    guint keyval = gdk_unicode_to_keyval(key_event.GetDomKey().ToCharacter());
    GdkKeymapKey* keys;
    guint* keyvals;
    int n_entries;

    gdk_display_map_keycode(GetDefaultGdkDisplay(), keycode, &keys, &keyvals,
                            &n_entries);

    for (int i = 0; i < n_entries; ++i) {
      if (keyvals[i] == keyval) {
        group = keys[i].group;
        break;
      }
    }

    g_free(keys);
    g_free(keyvals);
  }

  // As per Section 2.2.2 "Computing A State Field from an XKB State" of the XKB
  // protocol specification.
  return static_cast<GdkModifierType>(state | (group << 13));
}

int GtkUiPlatformWayland::GetGdkKeyEventGroup(const ui::KeyEvent& key_event) {
  auto state = GetGdkKeyEventState(key_event);
  // As per Section 2.2.2 "Computing A State Field from an XKB State" of the XKB
  // protocol specification.
  return (state >> 13) & 0x3;
}

GdkWindow* GtkUiPlatformWayland::GetGdkWindow(
    gfx::AcceleratedWidget window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
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
  // TODO(crbug.com/40650162): Check if gtk_window_present_with_time is needed
  // here as well, similarly to what is done in X11 impl.
  gtk_window_present(window);
}

GdkDisplay* GtkUiPlatformWayland::GetDefaultGdkDisplay() {
  if (!default_display_)
    default_display_ = gdk_display_get_default();
  return default_display_;
}

void GtkUiPlatformWayland::OnHandleSetTransient(GtkWidget* widget,
                                                const std::string& handle) {
  char* parent = const_cast<char*>(handle.c_str());
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
  // GDK3 doesn't have a way to create foreign wayland windows, so we can't
  // translate from ui::KeyEvent to GdkEventKey for InputMethodContextImplGtk.
  if (!GtkCheckVersion(4))
    return nullptr;
  return std::make_unique<InputMethodContextImplGtk>(delegate);
}

bool GtkUiPlatformWayland::IncludeFontScaleInDeviceScale() const {
  // Assume font scaling will be handled by Ozone/Wayland when WaylandUiScale
  // feature is enabled.
  return base::FeatureList::IsEnabled(features::kWaylandUiScale);
}

}  // namespace gtk
