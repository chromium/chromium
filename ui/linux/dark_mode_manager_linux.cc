// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/dark_mode_manager_linux.h"

#include <optional>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/base/ui_base_features.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

DarkModeManagerLinux::DarkModeManagerLinux()
    : DarkModeManagerLinux(dbus_thread_linux::GetSharedSessionBus(),
                           &ui::GetLinuxUiThemes()) {}

DarkModeManagerLinux::DarkModeManagerLinux(
    scoped_refptr<dbus::Bus> bus,
    const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>*
        linux_ui_themes)
    : linux_ui_themes_(linux_ui_themes),
      bus_(bus),
      settings_proxy_(bus_->GetObjectProxy(
          kFreedesktopSettingsService,
          dbus::ObjectPath(kFreedesktopSettingsObjectPath))) {
  dbus_xdg::RequestXdgDesktopPortal(
      bus_.get(), base::BindOnce(&DarkModeManagerLinux::OnPortalRequestResult,
                                 weak_ptr_factory_.GetWeakPtr()));
  // No seeding is needed: until the portal preference (if any) arrives, the
  // toolkit OsSettingsProviders source the toolkit-derived color scheme for
  // the web NativeTheme.
}

DarkModeManagerLinux::~DarkModeManagerLinux() = default;

// static
NativeTheme::PreferredColorScheme
DarkModeManagerLinux::FreedesktopColorSchemeToNativeThemeColorScheme(
    FreedesktopColorScheme color_scheme) {
  switch (color_scheme) {
    case FreedesktopColorScheme::kNoPreference:
      return NativeTheme::PreferredColorScheme::kNoPreference;
    case FreedesktopColorScheme::kDark:
      return NativeTheme::PreferredColorScheme::kDark;
    case FreedesktopColorScheme::kLight:
      return NativeTheme::PreferredColorScheme::kLight;
  }
  return NativeTheme::PreferredColorScheme::kNoPreference;
}

void DarkModeManagerLinux::OnPortalRequestResult(uint32_t version) {
  if (version == 0) {
    return;
  }
  // Subscribe to changes in the color scheme preference.
  dbus_utils::ConnectToSignal<"ssv">(
      settings_proxy_, kFreedesktopSettingsInterface, kSettingChangedSignal,
      base::BindRepeating(&DarkModeManagerLinux::OnPortalSettingChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DarkModeManagerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  // Read initial color scheme preference.
  dbus_utils::CallMethod<"ss", "v">(
      settings_proxy_, kFreedesktopSettingsInterface, kReadMethod,
      base::BindOnce(&DarkModeManagerLinux::OnReadColorScheme,
                     weak_ptr_factory_.GetWeakPtr()),
      kSettingsNamespace, kColorSchemeKey);

  // Read initial accent color preference.
  if (base::FeatureList::IsEnabled(features::kUsePortalAccentColor)) {
    dbus_utils::CallMethod<"ss", "v">(
        settings_proxy_, kFreedesktopSettingsInterface, kReadMethod,
        base::BindOnce(&DarkModeManagerLinux::OnReadAccentColor,
                       weak_ptr_factory_.GetWeakPtr()),
        kSettingsNamespace, kAccentColorKey);
  }
}

void DarkModeManagerLinux::OnSignalConnected(const std::string& interface_name,
                                             const std::string& signal_name,
                                             bool connected) {
  // Nothing to do.  Continue using the toolkit setting if !connected.
}

void DarkModeManagerLinux::OnPortalSettingChanged(
    dbus_utils::ConnectToSignalResultSig<"ssv"> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Received malformed Setting Changed signal from "
                  "org.freedesktop.portal.Settings";
    return;
  }

  auto& [namespace_changed, key_changed, value_variant] = result.value();

  if (namespace_changed != kSettingsNamespace) {
    return;
  }

  if (key_changed == kColorSchemeKey) {
    auto new_color_scheme = std::move(value_variant).Take<uint32_t>();
    if (!new_color_scheme.has_value()) {
      LOG(ERROR)
          << "Failed to read color-scheme value from SettingChanged signal";
      return;
    }

    SetColorScheme(FreedesktopColorSchemeToNativeThemeColorScheme(
        static_cast<FreedesktopColorScheme>(*new_color_scheme)));
  } else if (key_changed == kAccentColorKey &&
             base::FeatureList::IsEnabled(features::kUsePortalAccentColor)) {
    SetAccentColor(std::move(value_variant));
  }
}

void DarkModeManagerLinux::OnReadColorScheme(
    dbus_utils::CallMethodResultSig<"v"> result) {
  if (!result.has_value()) {
    // Continue using the toolkit setting.
    return;
  }

  auto& [outer_variant] = result.value();
  auto inner_variant = std::move(outer_variant).Take<dbus_utils::Variant>();
  if (!inner_variant.has_value()) {
    LOG(ERROR) << "Failed to read inner variant from Read method response";
    return;
  }

  auto new_color_scheme = std::move(*inner_variant).Take<uint32_t>();
  if (!new_color_scheme.has_value()) {
    LOG(ERROR) << "Failed to read color-scheme value from Read "
                  "method response";
    return;
  }

  SetColorScheme(FreedesktopColorSchemeToNativeThemeColorScheme(
      static_cast<FreedesktopColorScheme>(*new_color_scheme)));
}

void DarkModeManagerLinux::OnReadAccentColor(
    dbus_utils::CallMethodResultSig<"v"> result) {
  if (!result.has_value()) {
    // Continue using the toolkit setting.
    return;
  }

  auto& [outer_variant] = result.value();
  auto inner_variant = std::move(outer_variant).Take<dbus_utils::Variant>();
  if (!inner_variant.has_value()) {
    LOG(ERROR) << "Failed to read inner variant from Read method response";
    return;
  }

  SetAccentColor(std::move(*inner_variant));
}

void DarkModeManagerLinux::SetColorScheme(
    NativeTheme::PreferredColorScheme color_scheme) {
  // Convert to the toolkit-independent tri-state used by `LinuxUiTheme`:
  // `std::nullopt` (no preference) falls back to the toolkit-derived scheme in
  // the provider.
  std::optional<bool> prefer_dark;
  if (color_scheme == NativeTheme::PreferredColorScheme::kDark) {
    prefer_dark = true;
  } else if (color_scheme == NativeTheme::PreferredColorScheme::kLight) {
    prefer_dark = false;
  }

  // Push the portal color-scheme preference into each toolkit. `SetDarkTheme()`
  // toggles the toolkit's own dark/light variant for native widgets, while
  // `SetColorScheme()` routes the preference through the toolkit's
  // `OsSettingsProvider`, which is the single source of the web
  // `NativeTheme::preferred_color_scheme()` (via
  // `UpdateVariablesForToolkitSettings()`). Writing `preferred_color_scheme`
  // directly here would race with that provider-sourced write
  // (crbug.com/536445418).
  for (ui::LinuxUiTheme* linux_ui_theme : *linux_ui_themes_) {
    linux_ui_theme->SetDarkTheme(color_scheme ==
                                 NativeTheme::PreferredColorScheme::kDark);
    linux_ui_theme->SetColorScheme(prefer_dark);
  }
}

void DarkModeManagerLinux::SetAccentColor(dbus_utils::Variant variant) {
  auto color_tuple =
      std::move(variant).Take<std::tuple<double, double, double>>();
  if (!color_tuple.has_value()) {
    LOG(ERROR) << "Failed to read accent-color from variant";
    return;
  }

  auto& [r, g, b] = *color_tuple;
  bool valid = (r >= 0.0 && r <= 1.0) && (g >= 0.0 && g <= 1.0) &&
               (b >= 0.0 && b <= 1.0);

  std::optional<SkColor> accent_color;
  if (valid) {
    accent_color = SkColorSetRGB(r * 255, g * 255, b * 255);
  }

  // Push the accent color into the toolkit UI. Each toolkit routes it through
  // its `OsSettingsProvider` (`OsSettingsProviderGtk`/`OsSettingsProviderQt`),
  // which updates `NativeTheme::user_color()` via
  // `UpdateVariablesForToolkitSettings()` and notifies observers. Writing
  // `user_color` directly here would race with that provider-sourced write and
  // could be clobbered back to `nullopt` (crbug.com/536445418).
  for (ui::LinuxUiTheme* linux_ui_theme : *linux_ui_themes_) {
    linux_ui_theme->SetAccentColor(accent_color);
  }
}

}  // namespace ui
