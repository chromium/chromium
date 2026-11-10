// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/test/scoped_application_locale.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/number_formatting.h"
#include "base/scoped_environment_variable_override.h"
#include "build/build_config.h"

#if defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
#include <string>
#endif

namespace ui::test {

namespace {

std::optional<base::ScopedEnvironmentVariableOverride> MaybeOverrideLanguageEnv(
    const base::i18n::LanguageTag& locale) {
#if defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
  // On Linux with GLib, `l10n_util::GetApplicationLocale()` ignores the
  // `pref_locale` argument and resolves the locale from environment variables
  // starting with `LANGUAGE`.
  return std::make_optional<base::ScopedEnvironmentVariableOverride>(
      "LANGUAGE", std::string(locale.tag_string()));
#else
  return std::nullopt;
#endif
}

}  // namespace

ScopedApplicationLocale::ScopedApplicationLocale(
    const base::i18n::LanguageTag& locale)
    : language_override_(MaybeOverrideLanguageEnv(locale)),
      resource_bundle_locale_(locale),
      icu_locale_(locale) {
  CHECK_IS_TEST();
  base::ResetFormattersForTesting();
}

ScopedApplicationLocale::~ScopedApplicationLocale() {
  language_override_.reset();
  // The ICU default locale is restored after this body, when `icu_locale_` is
  // destroyed. This is fine as formatters are only recreated on their next use.
  base::ResetFormattersForTesting();
}

}  // namespace ui::test
