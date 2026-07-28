// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_compare.h"
#include "base/i18n/tag_converters.h"
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
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagMatcher;
using ::ui_l10n::GetAcceptLanguageMatcher;
using ::ui_l10n::GetAcceptLanguageTags;

std::string NormalizeLocaleWithLanguageTag(std::string_view locale) {
  return GetLanguageTagFromString(locale)
      .value_or(GetKnownLanguageTag("und"))
      .ToLegacyICUFormat();
}

// Returns true if `locale_name` has an alias in the ICU data file.
bool IsDuplicateName(std::string_view locale_name) {
  static constexpr auto kDuplicateNames =
      base::MakeFixedFlatSet<std::string_view>({
          "ar_001",
          "en",
          "en_001",
          "en_150",
          "pt",  // pt-BR and pt-PT are used.
          "zh",
          "zh_hans_cn",
          "zh_hant_hk",
          "zh_hant_mo",
          "zh_hans_sg",
          "zh_hant_tw",
      });

  // Skip all the es_Foo other than es_419 for now.
  if (base::StartsWith(locale_name, "es_",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return !locale_name.ends_with("419");
  }
  return kDuplicateNames.contains(base::ToLowerASCII(locale_name));
}

// 30+ minimally populated locales were added with only a few entries
// (exemplar character set, script, writing direction and its own
// lanaguage name). These locales have to be distinguished from the
// fully populated locales to which Chrome is localized.
bool IsLocalePartiallyPopulated(const std::string& locale_name) {
  // For partially populated locales, even the translation for "English"
  // is not available. A more robust/elegant way to check is to add a special
  // field (say, 'isPartial' to our version of ICU locale files) and
  // check its value, but this hack seems to work well.
  return !l10n_util::IsLocaleNameTranslated("en", locale_name);
}

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

struct AvailableLocalesTraits
    : base::internal::DestructorAtExitLazyInstanceTraits<
          std::vector<std::string>> {
  static std::vector<std::string>* New(void* instance) {
    std::vector<std::string>* locales =
        base::internal::DestructorAtExitLazyInstanceTraits<
            std::vector<std::string>>::New(instance);
    int num_locales = uloc_countAvailable();
    for (int i = 0; i < num_locales; ++i) {
      std::string locale_name = uloc_getAvailable(i);
      // Filter out the names that have aliases.
      if (IsDuplicateName(locale_name))
        continue;
      // Filter out locales for which only partially populated data is present
      // and to which Chrome is not localized.
      if (IsLocalePartiallyPopulated(locale_name))
        continue;
      // Normalize underscores to hyphens because that's what our locale files
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (base::EqualsCaseInsensitiveASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (base::EqualsCaseInsensitiveASCII(locale_name, "zh-hant")) {
        locale_name = "zh-TW";
      }
      locales->push_back(locale_name);
    }

    return locales;
  }
};

base::LazyInstance<std::vector<std::string>, AvailableLocalesTraits>
    g_available_locales = LAZY_INSTANCE_INITIALIZER;

}  // namespace

std::string_view GetLanguage(std::string_view locale) {
  return locale.substr(0, locale.find('-'));
}

std::string_view GetCountry(std::string_view locale) {
  size_t hyphen_pos = locale.find('-');
  return (hyphen_pos == std::string::npos) ? std::string_view()
                                           : locale.substr(hyphen_pos + 1);
}

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
  // Use any override (Cocoa for the browser), otherwise use the preference
  // passed to the function.
  std::string app_locale = l10n_util::GetLocaleOverride();
  if (app_locale.empty())
    app_locale = pref_locale;

  // The above should handle all of the cases Chrome normally hits, but for some
  // unit tests, fallback is needed too.
  if (app_locale.empty())
    app_locale = "en-US";

  return app_locale;
}
#endif

#if !BUILDFLAG(IS_APPLE)
std::string GetApplicationLocaleInternalNonMac(std::string_view pref_locale) {
  std::vector<std::optional<LanguageTag>> candidates;
  // The `prefered_tag` is separated from the other candidates.
  std::optional<LanguageTag> prefered_tag = std::nullopt;
  // Use --lang and the app pref on Windows.  On Linux, only
  // look at the LC_*/LANG environment variables.  However, passing --lang
  // to renderer and plugin processes is common, so they know what language the
  // parent process decided to use.

#if BUILDFLAG(IS_WIN)
  // First, try the preference value.
  if (!pref_locale.empty()) {
    prefered_tag = GetLanguageTagFromString(pref_locale);
  }

  // Next, try the overridden locale.
  const std::vector<std::string>& languages = l10n_util::GetLocaleOverrides();
  if (!languages.empty()) {
    candidates.reserve(candidates.size() + languages.size());
    std::ranges::transform(languages, std::back_inserter(candidates),
                           [](const std::string& language) {
                             return GetLanguageTagFromString(language);
                           });
  } else {
    // If no override was set, defer to ICU
    candidates.push_back(
        base::i18n::LanguageTagConverter::GetInstance().FromIcuLocale(
            icu::Locale::getDefault()));
  }
#elif BUILDFLAG(IS_ANDROID)
  // Try pref_locale first.
  if (!pref_locale.empty()) {
    prefered_tag = GetLanguageTagFromString(pref_locale);
  }

  // On Android, query java.util.Locale for the default locale.
  candidates.push_back(
      GetLanguageTagFromString(base::android::GetDefaultLocaleString()));
#elif defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS)
  // GLib implements correct environment variable parsing with
  // the precedence order: LANGUAGE, LC_ALL, LC_MESSAGES and LANG.
  const char* const* languages = g_get_language_names();
  DCHECK(languages);  // A valid pointer is guaranteed.
  DCHECK(*languages);  // At least one entry, "C", is guaranteed.

  // SAFETY: g_get_language_names returns a valid NULL-terminated array.
  // See: https://docs.gtk.org/glib/func.get_language_names.html
  for (; *languages; UNSAFE_BUFFERS(++languages)) {
    if (std::optional<LanguageTag> language_tag =
            GetLanguageTagFromString(*languages);
        language_tag) {
      candidates.push_back(std::move(language_tag));
    }
  }
#else
  // By default, use the application locale preference. This applies to ChromeOS
  // and linux systems without glib.
  if (!pref_locale.empty()) {
    prefered_tag = GetLanguageTagFromString(pref_locale);
  }
#endif  // BUILDFLAG(IS_WIN)

  // If `prefered_tag`, it is attempt to get a match for it, even if it is not
  // exact.
  if (prefered_tag) {
    if (std::optional<LanguageTag> resolved = CheckAndResolveLocale(
            *prefered_tag, CheckLocaleMode::kVerifyLocalizationDataExists)) {
      return std::string(resolved->tag_string());
    }
  }

  std::optional<LanguageTag> matched_candidate;
  for (const std::optional<LanguageTag>& candidate : candidates) {
    if (!candidate) {
      continue;
    }

    // If a exact match with a resource-bundle locale on-disk is found, it is
    // returned.
    if (IsResourceBundleLocale(*candidate)) {
      return std::string(candidate->tag_string());
    }

    if (matched_candidate) {
      continue;
    }
    // If there was a match using `CheckAndResolveLocale`, it is stored but not
    // returned yet because the priority is to find a candidate that has an
    // exact match with a `ResourceBundle` locale.
    if (std::optional<LanguageTag> resolved = CheckAndResolveLocale(
            *candidate, CheckLocaleMode::kVerifyLocalizationDataExists);
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
    base::i18n::SetICUDefaultLocale(locale);
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

std::u16string GetDisplayNameForLocaleWithoutCountry(
    std::string_view locale,
    std::string_view display_locale,
    bool is_for_ui,
    bool disallow_default) {
  return GetDisplayNameForLocale(GetLanguage(locale), display_locale, is_for_ui,
                                 disallow_default);
}

std::u16string GetDisplayNameForLocale(std::string_view locale,
                                       std::string_view display_locale,
                                       bool is_for_ui,
                                       bool disallow_default) {
  std::string locale_code = std::string(locale);
  std::string display_locale_code = std::string(display_locale);
  // Internally, zh-CN and zh-TW are used, but  the display names are supposed
  // to be Chinese (Simplified) and Chinese (Traditional) instead of Chinese
  // (China) and Chinese (Taiwan). Translate uses "tl" (Tagalog) to mean "fil"
  // (Filipino). Until Google translate is changed to understand "fil", make
  // "tl" alias to "fil". Translate also uses "gom" (Goan Konkani) for "kok"
  // (Konkani).
  if (locale_code == "gom") {
    locale_code = "kok";
  } else if (locale_code == "mo") {
    locale_code = "ro-MD";
  } else if (locale_code == "tl") {
    locale_code = "fil";
  } else if (locale_code == "zh-CN") {
    locale_code = "zh-Hans";
  } else if (locale_code == "zh-TW") {
    locale_code = "zh-Hant";
  }

  std::u16string display_name;

#if BUILDFLAG(ENABLE_PSEUDOLOCALES)
  if (locale_code == "en-XA") {
    return u"Long strings pseudolocale (en-XA)";
  } else if (locale_code == "ar-XB") {
    return u"RTL pseudolocale (ar-XB)";
  }
#endif  // BUILDFLAG(ENABLE_PSEUDOLOCALES)

#if BUILDFLAG(IS_IOS)
  // Use the Foundation API to get the localized display name, removing the need
  // for the ICU data file to include this data.
  display_name = GetDisplayNameForLocale(locale_code, display_locale_code);
#else
#if BUILDFLAG(IS_ANDROID)
  // Use Java API to get locale display name so it would be possible to remove
  // most of the lang data from icu data to reduce binary size, except for
  // zh-Hans and zh-Hant because the current Android Java API doesn't support
  // scripts.
  // TODO(wangxianzhu): remove the special handling of zh-Hans and zh-Hant once
  // Android Java API supports scripts.
  if (!locale_code.starts_with("zh-Han")) {
    display_name = GetDisplayNameForLocale(locale_code, display_locale_code);
  } else
#endif  // BUILDFLAG(IS_ANDROID)
  {
    UErrorCode error = U_ZERO_ERROR;
    const int kBufferSize = 1024;

    int32_t actual_size;
    // Country code in ICU64 is obtained by `uloc_getDisplayCountry`.
    if (locale_code[0] == '-' || locale_code[0] == '_') {
      actual_size = uloc_getDisplayCountry(
          locale_code.c_str(), display_locale_code.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    } else {
      actual_size = uloc_getDisplayName(
          locale_code.c_str(), display_locale_code.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    }
    if (disallow_default && U_USING_DEFAULT_WARNING == error)
      return std::u16string();
    DCHECK(U_SUCCESS(error));
    display_name.resize(base::checked_cast<size_t>(actual_size));
  }
#endif  // BUILDFLAG(IS_IOS)

  // Add directional markup so parentheses are properly placed.
  if (is_for_ui && base::i18n::IsRTL())
    base::i18n::AdjustStringForLocaleDirection(&display_name);
  return display_name;
}

std::u16string GetDisplayNameForCountry(std::string_view country_code,
                                        std::string_view display_locale) {
  if (country_code.empty()) {
    return std::u16string();
  }
  return GetDisplayNameForLocale(base::StrCat({"_", country_code}),
                                 display_locale, false);
}

std::vector<std::string> GetParentLocales(std::string_view current_locale) {
  std::string locale = NormalizeLocaleWithLanguageTag(current_locale);

  const int kNameCapacity = 256;
  char parent[kNameCapacity];
  base::strlcpy(parent, locale.c_str(), kNameCapacity);
  std::vector<std::string> parent_locales = {parent};
  UErrorCode err = U_ZERO_ERROR;
  while (uloc_getParent(parent, parent, kNameCapacity, &err) > 0) {
    if (U_FAILURE(err))
      break;
    parent_locales.push_back(parent);
  }
  return parent_locales;
}



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

const std::vector<std::string>& GetAvailableICULocales() {
  return g_available_locales.Get();
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

  const std::string_view language = l10n_util::GetLanguage(locale);

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
