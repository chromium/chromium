// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/test/scoped_resource_bundle_locale.h"

#include <string>

#include "base/check_is_test.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"

#if !BUILDFLAG(IS_ANDROID)
#include <optional>

#include "base/files/file_path.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace ui::test {

namespace {

// Reloads `ui::ResourceBundle` with `locale` and returns the loaded locale
// name without leaving the default ICU locale modified.
std::string ReloadResourceBundleLocale(const std::string& locale) {
  // `ResourceBundle::ReloadLocaleResources()` calls
  // `l10n_util::GetApplicationLocale()` with `set_icu_locale = true`, which
  // mutates the default ICU locale even when loading the `.pak` file fails.
  // Restore the previous default ICU locale so `ScopedResourceBundleLocale`
  // only changes `ui::ResourceBundle`.
  base::i18n::ScopedDefaultIcuLocale preserve_icu_locale(
      base::i18n::GetDefaultIcuLocale());
#if !BUILDFLAG(IS_ANDROID)
  // Resolve `locale` against the available `.pak` files on disk and pass the
  // resolved `.pak` path via `OverrideLocalePakForTest()` so reloading works
  // even when `l10n_util::GetApplicationLocale()` ignores `pref_locale` (e.g.
  // on Linux with GLib, or on Apple when a Cocoa locale override is set).
  std::optional<std::string> resolved = l10n_util::CheckAndResolveLocale(
      locale, l10n_util::CheckLocaleMode::kVerifyLocalizationDataExists);
  if (!resolved.has_value()) {
    return std::string();
  }
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  const base::FilePath pak_path =
      ui::ResourceBundle::GetLocaleFilePath(*resolved);
  if (pak_path.empty()) {
    return std::string();
  }
  const base::FilePath prev_overridden_pak_path = bundle.GetOverriddenPakPath();
  bundle.OverrideLocalePakForTest(pak_path);
  bundle.ReloadLocaleResources(*resolved);
  bundle.OverrideLocalePakForTest(prev_overridden_pak_path);
  return *resolved;
#else
  return ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);
#endif
}

}  // namespace

ScopedResourceBundleLocale::ScopedResourceBundleLocale(
    const base::i18n::LanguageTag& locale) {
  CHECK_IS_TEST();
  if (!ui::ResourceBundle::HasSharedInstance()) {
    return;
  }
  std::string current_locale =
      ui::ResourceBundle::GetSharedInstance().GetLoadedLocale();
#if BUILDFLAG(IS_ANDROID)
  // On Android, `ResourceBundle::LoadLocaleResources()` does not populate
  // `loaded_locale_`, so fall back to the ICU default locale when reloading.
  if (current_locale.empty()) {
    current_locale =
        std::string(base::i18n::GetDefaultIcuLocale().tag_string());
  }
#endif
  if (current_locale.empty()) {
    return;
  }
  prev_locale_ = current_locale;
  // If the requested locale's pak file cannot be loaded, fall back to the
  // previous locale so resources remain valid.
  if (ReloadResourceBundleLocale(std::string(locale.tag_string())).empty()) {
    ReloadResourceBundleLocale(prev_locale_);
    prev_locale_.clear();
  }
}

ScopedResourceBundleLocale::~ScopedResourceBundleLocale() {
  if (!prev_locale_.empty() && ui::ResourceBundle::HasSharedInstance()) {
    ReloadResourceBundleLocale(prev_locale_);
  }
}

}  // namespace ui::test
