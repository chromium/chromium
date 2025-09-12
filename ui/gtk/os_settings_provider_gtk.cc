// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/os_settings_provider_gtk.h"

#include <glib-object.h>
#include <glib.h>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

OsSettingsProviderGtk::OsSettingsProviderGtk()
    : OsSettingsProvider(PriorityLevel::kProduction) {}

OsSettingsProviderGtk::~OsSettingsProviderGtk() = default;

base::TimeDelta OsSettingsProviderGtk::CaretBlinkInterval() const {
  // Default value for `gtk-cursor-blink-time` from
  // https://docs.gtk.org/gtk3/property.Settings.gtk-cursor-blink-time.html.
  gint cursor_blink_time = 1200;
  gboolean cursor_blink = TRUE;
  g_object_get(gtk_settings_get_default(), "gtk-cursor-blink-time",
               &cursor_blink_time, "gtk-cursor-blink", &cursor_blink, nullptr);
  return cursor_blink ? base::Milliseconds(cursor_blink_time / 2)
                      : base::TimeDelta();
}

ScopedGSignal OsSettingsProviderGtk::ConnectSignal(const gchar* name) {
  return ScopedGSignal(gtk_settings_get_default(), name,
                       base::BindRepeating(&OsSettingsProviderGtk::OnSignal,
                                           base::Unretained(this)),
                       G_CONNECT_AFTER);
}

void OsSettingsProviderGtk::OnSignal(GtkSettings*, GtkParamSpec*) {
  NotifyOnSettingsChanged();
}

}  // namespace gtk
