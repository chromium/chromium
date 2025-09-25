// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_OS_SETTINGS_PROVIDER_GTK_H_
#define UI_GTK_OS_SETTINGS_PROVIDER_GTK_H_

#include <glib-object.h>

#include <array>

#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/native_theme/os_settings_provider.h"

using GtkParamSpec = struct _GtkParamSpec;
using GtkSettings = struct _GtkSettings;

namespace gtk {

class OsSettingsProviderGtk : public ui::OsSettingsProvider {
 public:
  OsSettingsProviderGtk();
  OsSettingsProviderGtk(const OsSettingsProviderGtk&) = delete;
  OsSettingsProviderGtk& operator=(const OsSettingsProviderGtk&) = delete;
  ~OsSettingsProviderGtk() override;

  // ui::OsSettingsProvider:
  ui::NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  ui::NativeTheme::PreferredContrast PreferredContrast() const override;
  base::TimeDelta CaretBlinkInterval() const override;

 private:
  ScopedGSignal ConnectSignal(const gchar* name);

  // Trampoline that invokes `NotifyOnSettingsChanged()`.
  void OnSignal(GtkSettings*, GtkParamSpec*);

  // Have to explicitly give template params instead of using `std::to_array()`,
  // since CTAD is banned in non-static member declarations :(
  std::array<ScopedGSignal, 4> signals_{
      ConnectSignal("notify::gtk-application-prefer-dark-theme"),
      ConnectSignal("notify::gtk-cursor-blink"),
      ConnectSignal("notify::gtk-cursor-blink-time"),
      ConnectSignal("notify::gtk-theme-name"),
  };
};

}  // namespace gtk

#endif  // UI_GTK_OS_SETTINGS_PROVIDER_GTK_H_
