// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/icu4c_tag_converter.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_compare.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/unicodestring.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/chromium_language_matcher.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#include "ui/base/l10n/l10n_util_android.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "ui/base/l10n/l10n_util_ios.h"
#endif

#if defined(USE_GLIB)
#include <glib.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/logging.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace l10n_util {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::IcuLocaleConverter;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagMatcher;
using ::ui_l10n::GetAcceptLanguageMatcher;
using ::ui_l10n::GetAcceptLanguageTags;

bool IsResourceBundleLocale(const LanguageTag& locale) {
  return ui::ResourceBundle::LocaleDataPakExists(
      locale, ui::ResourceBundle::Gender::kDefault);
}

// On Linux, the text layout engine Pango determines paragraph directionality
// by looking at the first strongly-directional character in the text. This
// means text such as "Google Chrome foo bar..." will be layed out LTR even
// if "foo bar" is RTL. So this function prepends the necessary RLM in such
// cases.
void AdjustParagraphDirectionality(std::u16string* paragraph) {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  if (base::i18n::IsRTL() &&
      base::i18n::StringContainsStrongRTLChars(*paragraph)) {
    paragraph->insert(0, 1, char16_t{base::i18n::kRightToLeftMark});
  }
#endif
}

std::u16string GetDisplayNameForLocaleInternal(
    const base::i18n::LanguageTag& locale,
    const base::i18n::LanguageTag& display_locale) {
  if (locale.tag_string() == "zh-TW") {
    return GetDisplayNameForLocaleInternal(GetKnownLanguageTag("zh-Hant"),
                                           display_locale);
  }
  if (locale.tag_string() == "zh-CN") {
    return GetDisplayNameForLocaleInternal(GetKnownLanguageTag("zh-Hans"),
                                           display_locale);
  }

#if BUILDFLAG(ENABLE_PSEUDOLOCALES)
  if (locale == GetKnownLanguageTag("en-XA")) {
    return u"Long strings pseudolocale (en-XA)";
  } else if (locale == GetKnownLanguageTag("ar-XB")) {
    return u"RTL pseudolocale (ar-XB)";
  }
#endif  // BUILDFLAG(ENABLE_PSEUDOLOCALES)

#if BUILDFLAG(IS_IOS)
  // Use the Foundation API to get the localized display name, removing the need
  // for the ICU data file to include this data.
  return GetDisplayNameForLocale(locale, display_locale);
#elif BUILDFLAG(IS_ANDROID)
  return GetDisplayNameForLocale(locale, display_locale);
#else   // BUILDFLAG(IS_ANDROID)
  icu::UnicodeString display_name_unicode;
  IcuLocaleConverter::GetInstance().FromLanguageTag(locale).getDisplayName(
      IcuLocaleConverter::GetInstance().FromLanguageTag(display_locale),
      display_name_unicode);
  return base::i18n::UnicodeStringToString16(display_name_unicode);
#endif  // BUILDFLAG(IS_IOS)
}

#if !BUILDFLAG(IS_APPLE)
// Use --lang and the app pref on Windows.  On Linux, only look at the LC_*/LANG
// environment variables.
std::vector<LanguageTag> GetCandidates() {
  std::vector<LanguageTag> candidates;
#if BUILDFLAG(IS_WIN)
  // Try the overridden locale.
  candidates = l10n_util::GetLocaleOverrides();
  if (candidates.empty()) {
    // If no override was set, defer to ICU
    return {base::i18n::IcuLocaleConverter::GetInstance().ToLanguageTag(
        icu::Locale::getDefault())};
  }
#elif BUILDFLAG(IS_ANDROID)
  // On Android, query java.util.Locale for the default locale.
  if (std::optional<LanguageTag> language_tag =
          GetLanguageTagFromString(base::android::GetDefaultLocaleString())) {
    candidates.push_back(*std::move(language_tag));
  }
#elif defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
  for (const char* var_name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
    const char* val = std::getenv(var_name);
    if (!val || *val == '\0') {
      continue;
    }
    for (std::string_view candidate : base::SplitStringPiece(
             val, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (std::optional<LanguageTag> language_tag =
              GetLanguageTagFromString(candidate)) {
        candidates.push_back(*std::move(language_tag));
      }
    }
  }
#endif  // BUILDFLAG(IS_WIN)
  return candidates;
}

#endif  // !BUILDFLAG(IS_APPLE)

// Preferred locales are enabled everywhere but Linux systems with GLib. This
// function parses `preferred_locale` into a `LanguageTag` if they are enabled.
std::optional<LanguageTag> GetPreferredTag(std::string_view preferred_locale) {
#if BUILDFLAG(IS_APPLE)
  std::string_view locale_override = l10n_util::GetLocaleOverride();
  return GetLanguageTagFromString(locale_override.empty() ? preferred_locale
                                                          : locale_override);
#elif defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
  return std::nullopt;
#else
  return GetLanguageTagFromString(preferred_locale);
#endif
}

}  // namespace

std::optional<LanguageTag> CheckAndResolveLocale(const LanguageTag& locale,
                                                 CheckLocaleMode mode) {
  if (mode == CheckLocaleMode::kVerifyLocalizationDataExists &&
      IsResourceBundleLocale(locale)) {
    return locale;
  }

  std::optional<LanguageTag> matched =
      ui_l10n::GetPlatformLanguageMatcher().Match(locale);
  if (!matched || (mode == CheckLocaleMode::kVerifyLocalizationDataExists &&
                   !IsResourceBundleLocale(*matched))) {
    return std::nullopt;
  }
  return matched;
}

std::optional<std::string> CheckAndResolveLocale(std::string_view locale,
                                                 CheckLocaleMode mode) {
  return GetLanguageTagFromString(locale).and_then(
      [mode](const LanguageTag& language_tag) {
        return CheckAndResolveLocale(language_tag, mode)
            .transform([](const LanguageTag& resolved) {
              return std::string(resolved.tag_string());
            });
      });
}

#if BUILDFLAG(IS_APPLE)
std::string GetApplicationLocaleInternalMac(std::string_view pref_locale) {
  std::optional<LanguageTag> preferred_locale_tag =
      GetPreferredTag(pref_locale);

  // The above should handle all of the cases Chrome normally hits, but for some
  // unit tests, fallback is needed too.
  if (!preferred_locale_tag) {
    return "en-US";
  }

  return std::string(preferred_locale_tag->tag_string());
}
#endif

#if !BUILDFLAG(IS_APPLE)
std::string GetApplicationLocaleInternalNonMac(std::string_view pref_locale) {
  // The `preferred_tag` is separated from the other candidates.
  std::optional<LanguageTag> preferred_tag = GetPreferredTag(pref_locale);
  // If `preferred_tag`, it attempts to get a match for it, even if it is not
  // exact.
  if (preferred_tag) {
    if (std::optional<LanguageTag> resolved = CheckAndResolveLocale(
            *preferred_tag, CheckLocaleMode::kVerifyLocalizationDataExists)) {
      return std::string(resolved->tag_string());
    }
  }

  std::vector<LanguageTag> candidates = GetCandidates();
  std::optional<LanguageTag> matched_candidate;
  for (const LanguageTag& candidate : candidates) {
    // If an exact match is found and resource-bundle data on-disk is found, it
    // is returned immediately.
    if (ui_l10n::GetPlatformLanguageMatcher().HasExactMatch(candidate) &&
        IsResourceBundleLocale(candidate)) {
      return std::string(candidate.tag_string());
    }

    if (matched_candidate) {
      continue;
    }
    // If there was a match using `CheckAndResolveLocale`, it is stored but not
    // returned yet because the priority is to find a candidate that has an
    // exact match with a `ResourceBundle` locale.
    if (std::optional<LanguageTag> resolved = CheckAndResolveLocale(
            candidate, CheckLocaleMode::kVerifyLocalizationDataExists);
        resolved) {
      matched_candidate = *resolved;
    }
  }

  if (matched_candidate) {
    return std::string(matched_candidate->tag_string());
  }

  // Fallback to "en-US"
  return IsResourceBundleLocale(GetKnownLanguageTag("en-US")) ? "en-US" : "";
}
#endif  // !BUILDFLAG(IS_APPLE)

std::string GetApplicationLocaleInternal(std::string_view pref_locale) {
#if BUILDFLAG(IS_APPLE)
  return GetApplicationLocaleInternalMac(pref_locale);
#else
  return GetApplicationLocaleInternalNonMac(pref_locale);
#endif
}

std::string GetApplicationLocale(std::string_view pref_locale,
                                 bool set_icu_locale) {
  const std::string locale = GetApplicationLocaleInternal(pref_locale);
  if (set_icu_locale && !locale.empty()) {
    std::optional<LanguageTag> language_tag = GetLanguageTagFromString(locale);
    if (language_tag) {
      base::i18n::SetDefaultIcuLocale(base::i18n::DefaultIcuLocaleSetterKey(),
                                      *language_tag);
    }
  }
  return locale;
}

bool IsLocaleNameTranslated(std::string_view locale,
                            std::string_view display_locale) {
  std::u16string display_name =
      l10n_util::GetDisplayNameForLocale(locale, display_locale, false);
  // Because ICU sets the error code to U_USING_DEFAULT_WARNING whether or not
  // uloc_getDisplayName returns the actual translation or the default
  // value (locale code), it is necessary to rely on this hack to tell whether
  // the translation is available or not.  If ICU doesn't have a translated
  // name for this locale, GetDisplayNameForLocale will just return the
  // locale code.
  return !base::IsStringASCII(display_name) ||
      base::UTF16ToASCII(display_name) != locale;
}

std::u16string GetDisplayNameForLocale(const LanguageTag& locale,
                                       const LanguageTag& display_locale,
                                       bool is_for_ui,
                                       bool disallow_default) {
  std::u16string display_name =
      GetDisplayNameForLocaleInternal(locale, display_locale);
  if (display_name.empty() && !disallow_default) {
    display_name =
        GetDisplayNameForLocaleInternal(locale, GetKnownLanguageTag("en-US"));
  }
  if (is_for_ui && base::i18n::IsRTL()) {
    base::i18n::AdjustStringForLocaleDirection(&display_name);
  }
  return display_name;
}

std::u16string GetDisplayNameForLocale(std::string_view locale,
                                       std::string_view display_locale,
                                       bool is_for_ui,
                                       bool disallow_default) {
  std::optional<LanguageTag> locale_tag = GetLanguageTagFromString(locale);
  std::optional<LanguageTag> display_locale_tag =
      GetLanguageTagFromString(display_locale);
  if (!locale_tag || !display_locale_tag) {
    return std::u16string();
  }

  return GetDisplayNameForLocale(*locale_tag, *display_locale_tag, is_for_ui,
                                 disallow_default);
}

std::u16string GetDisplayNameForLocaleWithoutCountry(
    std::string_view locale,
    std::string_view display_locale,
    bool is_for_ui,
    bool disallow_default) {
  std::optional<LanguageTag> locale_tag = GetLanguageTagFromString(locale);
  std::optional<LanguageTag> display_locale_tag =
      GetLanguageTagFromString(display_locale);

  if (!locale_tag || !display_locale_tag) {
    return std::u16string();
  }

  return GetDisplayNameForLocale(locale_tag->WithLanguageSubtagOnly(),
                                 *display_locale_tag, is_for_ui,
                                 disallow_default);
}

#if !BUILDFLAG(IS_IOS)
std::u16string GetDisplayNameForCountry(std::string_view country_code,
                                        std::string_view display_locale) {
  if (display_locale.empty()) {
    return std::u16string();
  }
  std::optional<LanguageTag> und_with_country =
      GetLanguageTagFromString(base::StrCat({"und-", country_code}));
  if (!und_with_country) {
    return std::u16string();
  }

  icu::Locale icu_display_locale =
      IcuLocaleConverter::GetInstance().FromLanguageTag(
          GetLanguageTagFromString(display_locale)
              .value_or(GetKnownLanguageTag("en-US")));
  icu::UnicodeString display_country;
  IcuLocaleConverter::GetInstance()
      .FromLanguageTag(*und_with_country)
      .getDisplayCountry(icu_display_locale, display_country);
  return base::i18n::UnicodeStringToString16(display_country);
}
#endif  // !BUILDFLAG(IS_IOS)

std::string GetStringUTF8(int message_id) {
  return base::UTF16ToUTF8(GetStringUTF16(message_id));
}

std::u16string GetStringUTF16(int message_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  std::u16string str = rb.GetLocalizedString(message_id);
  AdjustParagraphDirectionality(&str);

  return str;
}

std::u16string FormatString(const std::u16string& format_string,
                            const std::vector<std::u16string>& replacements,
                            std::vector<size_t>* offsets) {
#if DCHECK_IS_ON()
  // Make sure every replacement string is being used, so one is not inserted
  // silently.
  //
  // $9 is the highest allowed placeholder.
  for (size_t i = 0; i < 9; ++i) {
    bool placeholder_should_exist = i < replacements.size();

    std::u16string placeholder = u"$";
    placeholder += static_cast<char16_t>('1' + static_cast<char>(i));
    size_t pos = format_string.find(placeholder);
    if (placeholder_should_exist) {
      DCHECK_NE(std::string::npos, pos) << " Didn't find a " << placeholder
                                        << " placeholder in " << format_string;
    } else {
      DCHECK_EQ(std::string::npos, pos)
          << " Unexpectedly found a " << placeholder << " placeholder in "
          << format_string;
    }
  }
#endif

  // AdjustParagraphDirectionality() may append extra characters. Therefore,
  // it's important to AdjustParagraphDirectionality() before computing the
  // offsets in ReplaceStringPlaceholders(). Otherwise, offsets might be wrong.
  std::u16string formatted = format_string;
  AdjustParagraphDirectionality(&formatted);
  return base::ReplaceStringPlaceholders(formatted, replacements, offsets);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::vector<std::u16string>& replacements,
                               std::vector<size_t>* offsets) {
  // TODO(tc): saving a string copy here would be possible if the raw string
  // would be taken as a std::string_view and calling ReplaceStringPlaceholders
  // with a std::string_view format string and std::u16string substitution
  // strings was possible. In practice, the strings should be relatively short.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const std::u16string& format_string = rb.GetLocalizedString(message_id);
  return FormatString(format_string, replacements, offsets);
}

std::string GetStringFUTF8(int message_id, const std::u16string& a) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c,
                           const std::u16string& d) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c, d));
}

std::u16string GetStringFUTF16(int message_id, const std::u16string& a) {
  std::vector<std::u16string> replacements = {a};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b) {
  return GetStringFUTF16(message_id, a, b, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c) {
  std::vector<std::u16string> replacements = {a, b, c};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d) {
  std::vector<std::u16string> replacements = {a, b, c, d};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d,
                               const std::u16string& e) {
  std::vector<std::u16string> replacements = {a, b, c, d, e};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements = {a};
  std::u16string result = GetStringFUTF16(message_id, replacements, &offsets);
  DCHECK_EQ(1u, offsets.size());
  *offset = offsets[0];
  return result;
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               std::vector<size_t>* offsets) {
  std::vector<std::u16string> replacements = {a, b};
  return GetStringFUTF16(message_id, replacements, offsets);
}

std::u16string GetStringFUTF16Int(int message_id, int a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

std::u16string GetStringFUTF16Int(int message_id, int64_t a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

std::u16string GetPluralStringFUTF16(int message_id, int number) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), number);
}

std::string GetPluralStringFUTF8(int message_id, int number) {
  return base::UTF16ToUTF8(GetPluralStringFUTF16(message_id, number));
}

std::u16string GetSingleOrMultipleStringUTF16(int message_id,
                                              bool is_multiple) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), is_multiple ? "multiple" : "single");
}

void SortStrings16(const std::string& locale,
                   std::vector<std::u16string>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

bool IsUserFacingUILocale(std::string_view locale) {
  // As there are many callers of IsUserFacingUILocale and
  // GetUserFacingUILocaleList from threads where I/O is prohibited, do not
  // perform I/O here.
  const std::optional<std::string> resolved_locale =
      l10n_util::CheckAndResolveLocale(locale,
                                       CheckLocaleMode::kUseKnownLocalesList);
  if (!resolved_locale) {
    return false;
  }

  // Locales that have strings on disk should always be shown to the user.
  if (resolved_locale == locale) {
    return true;
  }

  std::optional<base::i18n::LanguageTag> language_tag =
      base::i18n::GetLanguageTagFromString(locale);
  if (!language_tag) {
    return false;
  }
  const std::string_view language = language_tag->language_subtag();

  // Chinese locales (other than the ones that have strings on disk) should not
  // be shown.
  if (base::EqualsCaseInsensitiveASCII(language, "zh")) {
    return false;
  }

  // Norwegian (no) should not be shown as it does not specify a written form.
  // Users can select Norwegian Bokmål (nb) or Norwegian Nynorsk (nn) instead.
  if (base::EqualsCaseInsensitiveASCII(language, "no")) {
    return false;
  }

  return true;
}

const std::vector<std::string>& GetUserFacingUILocaleList() {
  static base::NoDestructor<std::vector<std::string>> available_locales([] {
    std::vector<std::string> locales;
    for (const LanguageTag& tag : GetAcceptLanguageTags()) {
      if (IsUserFacingUILocale(tag.tag_string())) {
        locales.emplace_back(tag.tag_string());
      }
    }
    return locales;
  }());

  return *available_locales;
}

std::vector<std::string> GetAcceptLanguagesForLocale(
    std::string_view display_locale) {
  std::vector<std::string> result;
  for (const LanguageTag& tag : GetAcceptLanguageTags()) {
    if (!l10n_util::IsLocaleNameTranslated(tag.tag_string(), display_locale)) {
      // TODO(jungshik) : Put them at the end of the list with language codes
      // enclosed by brackets instead of skipping.
      continue;
    }
    result.emplace_back(tag.tag_string());
  }
  return result;
}

void GetAcceptLanguages(std::vector<std::string>* locale_codes) {
  for (const LanguageTag& tag : GetAcceptLanguageTags()) {
    locale_codes->emplace_back(tag.tag_string());
  }
}

bool IsPossibleAcceptLanguage(std::string_view locale) {
  std::optional<LanguageTag> tag = GetLanguageTagFromString(locale);
  if (!tag) {
    return false;
  }

  return GetAcceptLanguageMatcher().Match(*tag).has_value();
}

bool IsAcceptLanguageDisplayable(std::string_view display_locale,
                                 std::string_view locale) {
  return IsPossibleAcceptLanguage(locale) &&
         l10n_util::IsLocaleNameTranslated(locale, display_locale);
}

std::vector<std::string> KeepAcceptedLanguages(
    base::span<const std::string> languages) {
  std::vector<std::string> filtered_languages;
  std::ranges::copy_if(languages, std::back_inserter(filtered_languages),
                       IsPossibleAcceptLanguage);
  return filtered_languages;
}

int GetLocalizedContentsWidthInPixels(int pixel_resource_id) {
  int width = 0;
  base::StringToInt(l10n_util::GetStringUTF8(pixel_resource_id), &width);
  DCHECK_GT(width, 0);
  return width;
}

std::vector<std::string_view> GetAcceptLanguageListForTesting() {
  std::vector<std::string_view> result;
  for (const LanguageTag& tag : GetAcceptLanguageTags()) {
    result.push_back(tag.tag_string());
  }
  return result;
}

base::span<const LanguageTag> GetPlatformLocalesForTesting() {
  return ui_l10n::GetPlatformLanguageTags();
}

}  // namespace l10n_util
