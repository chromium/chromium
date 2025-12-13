// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/os_settings_provider_gtk.h"

#include <glib-object.h>
#include <glib.h>

#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/gfx/color_utils.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/native_theme.h"

namespace gtk {

OsSettingsProviderGtk::OsSettingsProviderGtk()
    : OsSettingsProvider(PriorityLevel::kProduction) {}

OsSettingsProviderGtk::~OsSettingsProviderGtk() = default;

ui::NativeTheme::PreferredColorScheme
OsSettingsProviderGtk::PreferredColorScheme() const {
  // GTK has a dark mode setting called "gtk-application-prefer-dark-theme", but
  // this is really only used for themes that have a dark or light variant that
  // gets toggled based on this setting (eg. Adwaita).  Most dark themes do not
  // have a light variant and aren't affected by the setting.  Because of this,
  // experimentally check if the theme is dark by checking if the window
  // background color is dark.
  return color_utils::IsDark(GetBgColor({}))
             ? ui::NativeTheme::PreferredColorScheme::kDark
             : ui::NativeTheme::PreferredColorScheme::kLight;
}

ui::NativeTheme::PreferredContrast OsSettingsProviderGtk::PreferredContrast()
    const {
  // GTK doesn't have a native high contrast setting.  Rather, it's implied by
  // the theme name.  The only high contrast GTK themes that I know of are
  // HighContrast (GNOME) and ContrastHighInverse (MATE).  So infer the contrast
  // based on if the theme name contains both "high" and "contrast",
  // case-insensitive.
  const std::string theme_name =
      base::ToLowerASCII(GetGtkSettingsStringProperty(
          gtk_settings_get_default(), "gtk-theme-name"));
  const bool high_contrast = base::Contains(theme_name, "high") &&
                             base::Contains(theme_name, "contrast");
  return high_contrast ? ui::NativeTheme::PreferredContrast::kMore
                       : ui::NativeTheme::PreferredContrast::kNoPreference;
}

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
