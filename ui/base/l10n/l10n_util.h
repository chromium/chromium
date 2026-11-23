// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with localized
// content.

#ifndef UI_BASE_L10N_L10N_UTIL_H_
#define UI_BASE_L10N_L10N_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "ui/base/l10n/localized_strings.h"  // IWYU pragma: export

#if BUILDFLAG(IS_APPLE)
#include "ui/base/l10n/l10n_util_mac.h"
#endif  // BUILDFLAG(IS_APPLE)

namespace base::i18n {
class LanguageTag;
}  // namespace base::i18n

namespace l10n_util {

enum class CheckLocaleMode {
  // Checks that the localization data is present on disk. It is the default,
  // but potentially costly.
  kVerifyLocalizationDataExists,
  // Checks that the locale is in the list of known locales. It may lead to
  // false positives on platforms where localization is downloaded on-demand
  // - i.e., Android and iOS. See the `kPlatformLocales` documentation in
  // l10n_util.cc for more information.
  kUseKnownLocalesList,
};

// Translates a generic locale name to one of the locally defined ones or
// `std::nullopt` if the resolution is unsuccessful.
COMPONENT_EXPORT(UI_BASE)
std::optional<std::string> CheckAndResolveLocale(
    std::string_view locale,
    CheckLocaleMode mode = CheckLocaleMode::kVerifyLocalizationDataExists);

// This method is responsible for determining the locale as defined below. In
// nearly all cases you shouldn't call this, rather use GetApplicationLocale
// defined on browser_process.
//
// Returns the locale used by the Application.  The algorithm follows this list
// of preferences to find a suitable locale:
// - First the value from the command line (--lang);
// - Second the value in the prefs file (passed in as `pref_locale`);
// - Finally, fallback on the system locale.
//
// A value is returned if it has a corresponding resource on-disk. Otherwise,
// "en-US" is used as the last fallback. `set_icu_locale` determines whether
// the resulting locale is set as the default ICU locale before returning it.
COMPONENT_EXPORT(UI_BASE)
std::string GetApplicationLocale(std::string_view pref_locale,
                                 bool set_icu_locale = true);

// This method returns the display name of the locale code in |display_locale|.
// For example, for |locale| = "en-US" and |display_locale| = "en",
// it returns "English (United States)". To get the display name of
// |locale| in the UI language of Chrome, |display_locale| can be
// set to the return value of g_browser_process->GetApplicationLocale()
// in the UI thread.
// If |is_for_ui| is true, U+200F is appended so that it can be
// rendered properly in a RTL Chrome.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForLocale(std::string_view locale,
                                       std::string_view display_locale,
                                       bool is_for_ui,
                                       bool disallow_default = false);

// This method behaves exactly like the string_view overload of
// GetDisplayNameForLocale but accepts base::i18n::LanguageTag inputs.
// It is strongly preferred to use this method over the string_view one when
// LanguageTag objects are already available, as it guarantees type safety and
// avoids redundant string copies or parsing.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForLocale(
    const base::i18n::LanguageTag& locale,
    const base::i18n::LanguageTag& display_locale,
    bool is_for_ui,
    bool disallow_default = false);

// Returns the display name of the `country_code` in `display_locale`.
// Returns an empty string if `country_code` is empty or invalid, or if
// `display_locale` is "und".
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForCountry(
    std::string_view country_code,
    const base::i18n::LanguageTag& display_locale);

// In place sorting of std::u16string strings using collation rules for
// |locale|.
COMPONENT_EXPORT(UI_BASE)
void SortStrings16(const std::string& locale,
                   std::vector<std::u16string>* strings);

// Returns whether a locale shouled be shown to the user as a supported UI
// locale. This is similar to CheckAndResolveLocale, except that it excludes
// some languages from being shown.
COMPONENT_EXPORT(UI_BASE)
bool IsUserFacingUILocale(const base::i18n::LanguageTag& locale);

// Returns the subset of locales from GetAcceptLanguages which should be shown
// to the user as a supported UI locale.
// E.g., a vector containing en-US, en-CA, en-GB, es, fr, pt-PT, pt-BR, etc.
COMPONENT_EXPORT(UI_BASE)
const std::vector<std::string>& GetUserFacingUILocaleList();

// Returns a vector of locale codes usable for accept-languages.
COMPONENT_EXPORT(UI_BASE)
std::vector<std::string> GetAcceptLanguagesForLocale(
    std::string_view display_locale);

// Same as `GetAcceptLanguagesForLocale()`, but takes and returns
// `LanguageTag`s.
COMPONENT_EXPORT(UI_BASE)
std::vector<base::i18n::LanguageTag> GetAcceptLanguageTagsForLocale(
    const base::i18n::LanguageTag& display_locale);

// Returns a vector of untranslated locale codes usable for accept-languages.
COMPONENT_EXPORT(UI_BASE)
void GetAcceptLanguages(std::vector<std::string>* locale_codes);

// Returns true if `locale` is in a predefined `kAcceptLanguageList` and
// a display name for the `locale` is available in the locale `display_locale`.
COMPONENT_EXPORT(UI_BASE)
bool IsAcceptLanguageDisplayable(const base::i18n::LanguageTag& display_locale,
                                 const base::i18n::LanguageTag& locale);

// Filters the input vector of languages. Returns only those in the
// `kAcceptLanguageList`.
COMPONENT_EXPORT(UI_BASE)
std::vector<base::i18n::LanguageTag> KeepAcceptedLanguages(
    base::span<const base::i18n::LanguageTag> languages);

COMPONENT_EXPORT(UI_BASE)
std::vector<std::string_view> GetAcceptLanguageListForTesting();

COMPONENT_EXPORT(UI_BASE)
base::span<const base::i18n::LanguageTag> GetPlatformLocalesForTesting();

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_H_
