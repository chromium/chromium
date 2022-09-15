// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SETTINGS_PROVIDER_GSETTINGS_H_
#define UI_GTK_SETTINGS_PROVIDER_GSETTINGS_H_

#include <gio/gio.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/gtk/settings_provider.h"

namespace gtk {
class GtkUi;

// On GNOME desktops, subscribes to the gsettings key which controlls button
// order and the middle click action. Everywhere else, SetTiltebarButtons()
// just calls back into BrowserTitlebar with the default ordering.
class SettingsProviderGSettings : public SettingsProvider {
 public:
  // Sends data to the GtkUi when available.
  explicit SettingsProviderGSettings(GtkUi* delegate);

  SettingsProviderGSettings(const SettingsProviderGSettings&) = delete;
  SettingsProviderGSettings& operator=(const SettingsProviderGSettings&) =
      delete;

  ~SettingsProviderGSettings() override;

 private:
  CHROMEG_CALLBACK_1(SettingsProviderGSettings,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GSettings*,
                     const gchar*);

  CHROMEG_CALLBACK_1(SettingsProviderGSettings,
                     void,
                     OnMiddleClickActionChanged,
                     GSettings*,
                     const gchar*);

  void ParseAndStoreButtonValue(const std::string&);

  void ParseAndStoreMiddleClickValue(const std::string&);

  raw_ptr<GtkUi> delegate_;

  ScopedGObject<GSettings> button_settings_;
  ScopedGObject<GSettings> click_settings_;
  gulong signal_button_id_;
  gulong signal_middle_click_id_;
};

}  // namespace gtk

#endif  // UI_GTK_SETTINGS_PROVIDER_GSETTINGS_H_
