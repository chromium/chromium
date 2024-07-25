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
#include "base/containers/span.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/base/l10n/l10n_util_mac.h"
#endif  // BUILDFLAG(IS_APPLE)

namespace l10n_util {

// Takes normalized locale as |locale|. Returns language part (before '-').
COMPONENT_EXPORT(UI_BASE) std::string GetLanguage(std::string_view locale);

// Takes normalized locale as |locale|. Returns country part (after '-').
COMPONENT_EXPORT(UI_BASE) std::string GetCountry(std::string_view locale);

// This method translates a generic locale name to one of the locally defined
// ones. This method returns true if it succeeds.
// If |perform_io| is false, this will not perform any I/O but may return false
// positives on Android and iOS. See the |kPlatformLocales| documentation in
// l10n_util.cc for more information.
COMPONENT_EXPORT(UI_BASE)
bool CheckAndResolveLocale(const std::string& locale,
                           std::string* resolved_locale,
                           const bool perform_io);

// Convenience wrapper for the above (with |perform_io| set to true).
COMPONENT_EXPORT(UI_BASE)
bool CheckAndResolveLocale(const std::string& locale,
                           std::string* resolved_locale);

// This method is responsible for determining the locale as defined below. In
// nearly all cases you shouldn't call this, rather use GetApplicationLocale
// defined on browser_process.
//
// Returns the locale used by the Application.  First we use the value from the
// command line (--lang), second we try the value in the prefs file (passed in
// as |pref_locale|), finally, we fall back on the system locale. We only return
// a value if there's a corresponding resource DLL for the locale.  Otherwise,
// we fall back to en-us. |set_icu_locale| determines whether the resulting
// locale is set as the default ICU locale before returning it.
COMPONENT_EXPORT(UI_BASE)
std::string GetApplicationLocale(const std::string& pref_locale,
                                 bool set_icu_locale);

// Convenience version of GetApplicationLocale() that sets the resulting locale
// as the default ICU locale before returning it.
COMPONENT_EXPORT(UI_BASE)
std::string GetApplicationLocale(const std::string& pref_locale);

// Returns true if a display name for |locale| is available in the locale
// |display_locale|.
COMPONENT_EXPORT(UI_BASE)
bool IsLocaleNameTranslated(std::string_view locale,
                            std::string_view display_locale);

// This method returns the display name of the `locale` code in `display_locale`
// without the country. For example, for `locale` = "en-US" and `display_locale`
// = "en", it returns "English" in English, per "en". Chrome has different
// strings for some languages depending on the locale. To get the display name
// of `locale` in the UI language of Chrome, `display_locale` can be set to the
// return value of g_browser_process->GetApplicationLocale() in the UI thread.
// If `is_for_ui` is true, U+200F is appended so that it can be rendered
// properly in a RTL Chrome.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForLocaleWithoutCountry(
    std::string_view locale,
    std::string_view display_locale,
    bool is_for_ui,
    bool disallow_default = false);

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

// Returns the display name of the |country_code| in |display_locale|.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetDisplayNameForCountry(const std::string& country_code,
                                        const std::string& display_locale);

// Converts all - into _, to be consistent with ICU and file system names.
COMPONENT_EXPORT(UI_BASE)
std::string NormalizeLocale(const std::string& locale);

// Produce a vector of parent locales for given locale.
// It includes the current locale in the result.
// sr_Cyrl_RS generates sr_Cyrl_RS, sr_Cyrl and sr.
COMPONENT_EXPORT(UI_BASE)
void GetParentLocales(const std::string& current_locale,
                      std::vector<std::string>* parent_locales);

// Checks if a string is plausibly a syntactically-valid locale string,
// for cases where we want the valid input to be a locale string such as
// 'en', 'pt-BR', 'fil', 'es-419', 'zh-Hans-CN', 'i-klingon' or
// 'de_DE@collation=phonebook', but we don't want to limit it to
// locales that Chrome actually knows about, so 'xx-YY' should be
// accepted, but 'z', 'German', 'en-$1', or 'abcd-1234' should not.
// Case-insensitive. Based on BCP 47, see:
//   http://unicode.org/reports/tr35/#Unicode_Language_and_Locale_Identifiers
COMPONENT_EXPORT(UI_BASE) bool IsValidLocaleSyntax(const std::string& locale);

//
// Mac Note: See l10n_util_mac.h for some NSString versions and other support.
//

// Pulls resource string from the string bundle and returns it.
COMPONENT_EXPORT(UI_BASE) std::string GetStringUTF8(int message_id);
COMPONENT_EXPORT(UI_BASE) std::u16string GetStringUTF16(int message_id);

// Given a format string, replace $i with replacements[i] for all
// i < replacements.size(). Additionally, $$ is replaced by $.
// If non-NULL |offsets| will be replaced with the start points of the replaced
// strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string FormatString(const std::u16string& format_string,
                            const std::vector<std::u16string>& replacements,
                            std::vector<size_t>* offsets);

// Get a resource string and replace $i with replacements[i] for all
// i < replacements.size(). Additionally, $$ is replaced by $.
// If non-NULL |offsets| will be replaced with the start points of the replaced
// strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::vector<std::u16string>& replacements,
                               std::vector<size_t>* offsets);

// Convenience wrappers for the above.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d,
                               const std::u16string& e);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id, const std::u16string& a);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c);
COMPONENT_EXPORT(UI_BASE)
std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c,
                           const std::u16string& d);

// Variants that return the offset(s) of the replaced parameters. The
// vector based version returns offsets ordered by parameter. For example if
// invoked with a and b offsets[0] gives the offset for a and offsets[1] the
// offset of b regardless of where the parameters end up in the string.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               size_t* offset);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               std::vector<size_t>* offsets);

// Convenience functions to get a string with a single integer as a parameter.
// The result will use non-ASCII(native) digits if required by a locale
// convention (e.g. Persian, Bengali).
// If a message requires plural formatting (e.g. "3 tabs open"), use
// GetPluralStringF*, instead. To format a double, integer or percentage alone
// without any surrounding text (e.g. "3.57", "123", "45%"), use
// base::Format{Double,Number,Percent}. With more than two numbers or
// number + surrounding text, use base::i18n::MessageFormatter.
// // Note that native digits have to be used in UI in general.
// base::{Int*,Double}ToString convert a number to a string with
// ASCII digits in non-UI strings.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16Int(int message_id, int a);
COMPONENT_EXPORT(UI_BASE)
std::u16string GetStringFUTF16Int(int message_id, int64_t a);

// Convenience functions to format a string with a single number that requires
// plural formatting. Note that a simple 2-way rule (singular vs plural)
// breaks down for a number of languages. Instead of two separate messages
// for singular and plural, use this method with one message in ICU syntax.
// See http://userguide.icu-project.org/formatparse/messages and
// go/plurals (Google internal) for more details and examples.
//
// For complex messages with input parameters of multiple types (int,
// double, time, string; e.g. "At 3:45 on Feb 3, 2016, 5 files are downloaded
// at 3 MB/s."), use base::i18n::MessageFormatter.
// message_format_unittests.cc also has more examples of plural formatting.
COMPONENT_EXPORT(UI_BASE)
std::u16string GetPluralStringFUTF16(int message_id, int number);
COMPONENT_EXPORT(UI_BASE)
std::string GetPluralStringFUTF8(int message_id, int number);

// Get a string when you only care about 'single vs multiple' distinction.
// The message pointed to by |message_id| should be in ICU syntax
// (see the references above for Plural) with 'single', 'multiple', and
// 'other' (fallback) instead of 'male', 'female', and 'other' (fallback).
COMPONENT_EXPORT(UI_BASE)
std::u16string GetSingleOrMultipleStringUTF16(int message_id, bool is_multiple);

// In place sorting of std::u16string strings using collation rules for
// |locale|.
COMPONENT_EXPORT(UI_BASE)
void SortStrings16(const std::string& locale,
                   std::vector<std::u16string>* strings);

// Returns a vector of available locale codes from ICU. E.g., a vector
// containing en-US, es, fr, fi, pt-PT, pt-BR, etc.
COMPONENT_EXPORT(UI_BASE)
const std::vector<std::string>& GetAvailableICULocales();

// Returns whether we should show a locale to the user as a supported UI locale.
// This is similar to CheckAndResolveLocale, except that it excludes some
// languages from being shown.
COMPONENT_EXPORT(UI_BASE)
bool IsUserFacingUILocale(const std::string& locale);

// Returns the subset of locales from GetAcceptLanguages which we should show
// to the user as a supported UI locale.
// E.g., a vector containing en-US, en-CA, en-GB, es, fr, pt-PT, pt-BR, etc.
COMPONENT_EXPORT(UI_BASE)
const std::vector<std::string>& GetUserFacingUILocaleList();

// Returns a vector of locale codes usable for accept-languages.
COMPONENT_EXPORT(UI_BASE)
void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes);

// Returns a vector of untranslated locale codes usable for accept-languages.
COMPONENT_EXPORT(UI_BASE)
void GetAcceptLanguages(std::vector<std::string>* locale_codes);

// Returns true if |locale| is in a predefined |AcceptLanguageList|.
COMPONENT_EXPORT(UI_BASE)
bool IsPossibleAcceptLanguage(std::string_view locale);

// Returns true if |locale| is in a predefined |AcceptLanguageList| and
// a display name for the |locale| is available in the locale |display_locale|.
COMPONENT_EXPORT(UI_BASE)
bool IsAcceptLanguageDisplayable(const std::string& display_locale,
                                 const std::string& locale);

// Filters the input vector of languages. Returns only those in the
// |AcceptLanguageList|.
COMPONENT_EXPORT(UI_BASE)
std::vector<std::string> KeepAcceptedLanguages(
    base::span<const std::string> languages);

// Returns the preferred size of the contents view of a window based on
// designer given constraints which might dependent on the language used.
COMPONENT_EXPORT(UI_BASE)
int GetLocalizedContentsWidthInPixels(int pixel_resource_id);

COMPONENT_EXPORT(UI_BASE)
std::vector<std::string_view> GetAcceptLanguageListForTesting();

COMPONENT_EXPORT(UI_BASE) const char* const* GetPlatformLocalesForTesting();

COMPONENT_EXPORT(UI_BASE) size_t GetPlatformLocalesSizeForTesting();

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_H_
