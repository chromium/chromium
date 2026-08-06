// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_DARK_MODE_MANAGER_LINUX_H_
#define UI_LINUX_DARK_MODE_MANAGER_LINUX_H_

#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/variant.h"
#include "ui/native_theme/native_theme.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace ui {

class LinuxUiTheme;
class DarkModeManagerLinuxTest;

// Reads the system color-scheme and accent-color preferences from
// org.freedesktop.portal.Settings and pushes them into each toolkit's
// `OsSettingsProvider` (`OsSettingsProviderGtk`/`OsSettingsProviderQt`), which
// source the corresponding web `NativeTheme` values. When the portal is
// unavailable, the providers fall back to the toolkit-derived values.
class DarkModeManagerLinux {
 public:
  DarkModeManagerLinux();
  DarkModeManagerLinux(
      scoped_refptr<dbus::Bus> bus,
      const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>*
          linux_ui_themes);
  DarkModeManagerLinux(const DarkModeManagerLinux&) = delete;
  DarkModeManagerLinux& operator=(const DarkModeManagerLinux&) = delete;
  ~DarkModeManagerLinux();

 private:
  friend class DarkModeManagerLinuxTest;
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UseNativeThemeSetting);
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UsePortalSetting);
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UsePortalAccentColor);

  constexpr static char kFreedesktopSettingsService[] =
      "org.freedesktop.portal.Desktop";
  constexpr static char kFreedesktopSettingsObjectPath[] =
      "/org/freedesktop/portal/desktop";
  constexpr static char kFreedesktopSettingsInterface[] =
      "org.freedesktop.portal.Settings";
  constexpr static char kSettingChangedSignal[] = "SettingChanged";
  constexpr static char kReadMethod[] = "Read";
  constexpr static char kSettingsNamespace[] = "org.freedesktop.appearance";
  constexpr static char kColorSchemeKey[] = "color-scheme";
  constexpr static char kAccentColorKey[] = "accent-color";

  enum class FreedesktopColorScheme {
    // These constants are defined by the org.freedesktop.portal.Settings spec.
    kNoPreference = 0,
    kDark = 1,
    kLight = 2,
  };

  static NativeTheme::PreferredColorScheme
  FreedesktopColorSchemeToNativeThemeColorScheme(
      DarkModeManagerLinux::FreedesktopColorScheme color_scheme);

  // D-Bus async handlers
  void OnPortalRequestResult(uint32_t version);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);
  void OnPortalSettingChanged(
      dbus_utils::ConnectToSignalResultSig<"ssv"> result);
  void OnReadColorScheme(dbus_utils::CallMethodResultSig<"v"> result);
  void OnReadAccentColor(dbus_utils::CallMethodResultSig<"v"> result);

  // Pushes the portal color-scheme preference into each toolkit's
  // `OsSettingsProvider`, which sources the web theme.
  void SetColorScheme(NativeTheme::PreferredColorScheme color_scheme);

  void SetAccentColor(dbus_utils::Variant variant);

  raw_ptr<const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>>
      linux_ui_themes_;

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> settings_proxy_;

  base::WeakPtrFactory<DarkModeManagerLinux> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_LINUX_DARK_MODE_MANAGER_LINUX_H_
