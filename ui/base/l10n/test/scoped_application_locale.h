// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_TEST_SCOPED_APPLICATION_LOCALE_H_
#define UI_BASE_L10N_TEST_SCOPED_APPLICATION_LOCALE_H_

#include <optional>

#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/scoped_environment_variable_override.h"
#include "ui/base/l10n/test/scoped_resource_bundle_locale.h"

namespace ui::test {

// Test helper that changes the application locale during its lifetime, i.e.
// the ICU default locale (see `base::i18n::ScopedDefaultIcuLocale`), the
// `ui::ResourceBundle` localized resources (see
// `ScopedResourceBundleLocale`), and, on Linux with GLib, the `LANGUAGE`
// environment variable read by `l10n_util::GetApplicationLocale()`, and
// restores them upon destruction. It also resets the cached `base` number
// formatters so that they use the current ICU default locale.
class ScopedApplicationLocale {
 public:
  explicit ScopedApplicationLocale(const base::i18n::LanguageTag& locale);
  ~ScopedApplicationLocale();

  ScopedApplicationLocale(const ScopedApplicationLocale&) = delete;
  ScopedApplicationLocale& operator=(const ScopedApplicationLocale&) = delete;

 private:
  // Populated on Linux with GLib to override `LANGUAGE`. Declared before
  // `resource_bundle_locale_` so `LANGUAGE` is set before reloading resources,
  // and reset in `~ScopedApplicationLocale()` before `resource_bundle_locale_`
  // restores the previous resources.
  std::optional<base::ScopedEnvironmentVariableOverride> language_override_;
  // Declared before `icu_locale_` so that, on Android, it can read the ICU
  // default locale before `icu_locale_` changes it.
  ScopedResourceBundleLocale resource_bundle_locale_;
  base::i18n::ScopedDefaultIcuLocale icu_locale_;
};

}  // namespace ui::test

#endif  // UI_BASE_L10N_TEST_SCOPED_APPLICATION_LOCALE_H_
