// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_compare.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "base/android/locale_utils.h"
#include "ui/base/l10n/l10n_util_android.h"
#endif

#if defined(USE_GLIB)
#include <glib.h>
#endif

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif  // OS_WIN

namespace {

static const char* const kAcceptLanguageList[] = {
    "af",     // Afrikaans
    "am",     // Amharic
    "an",     // Aragonese
    "ar",     // Arabic
    "ast",    // Asturian
    "az",     // Azerbaijani
    "be",     // Belarusian
    "bg",     // Bulgarian
    "bh",     // Bihari
    "bn",     // Bengali
    "br",     // Breton
    "bs",     // Bosnian
    "ca",     // Catalan
    "ceb",    // Cebuano
    "ckb",    // Kurdish (Arabci),  Sorani
    "co",     // Corsican
    "cs",     // Czech
    "cy",     // Welsh
    "da",     // Danish
    "de",     // German
    "de-AT",  // German (Austria)
    "de-CH",  // German (Switzerland)
    "de-DE",  // German (Germany)
    "de-LI",  // German (Liechtenstein)
    "el",     // Greek
    "en",     // English
    "en-AU",  // English (Australia)
    "en-CA",  // English (Canada)
    "en-GB",  // English (UK)
    "en-IN",  // English (India)
    "en-NZ",  // English (New Zealand)
    "en-US",  // English (US)
    "en-ZA",  // English (South Africa)
    "eo",     // Esperanto
    // TODO(jungshik) : Do we want to list all es-Foo for Latin-American
    // Spanish speaking countries?
    "es",      // Spanish
    "es-419",  // Spanish (Latin America)
    "es-AR",   // Spanish (Argentina)
    "es-CL",   // Spanish (Chile)
    "es-CO",   // Spanish (Colombia)
    "es-CR",   // Spanish (Costa Rica)
    "es-ES",   // Spanish (Spain)
    "es-HN",   // Spanish (Honduras)
    "es-MX",   // Spanish (Mexico)
    "es-PE",   // Spanish (Peru)
    "es-US",   // Spanish (US)
    "es-UY",   // Spanish (Uruguay)
    "es-VE",   // Spanish (Venezuela)
    "et",      // Estonian
    "eu",      // Basque
    "fa",      // Persian
    "fi",      // Finnish
    "fil",     // Filipino
    "fo",      // Faroese
    "fr",      // French
    "fr-CA",   // French (Canada)
    "fr-CH",   // French (Switzerland)
    "fr-FR",   // French (France)
    "fy",      // Frisian
    "ga",      // Irish
    "gd",      // Scots Gaelic
    "gl",      // Galician
    "gn",      // Guarani
    "gu",      // Gujarati
    "ha",      // Hausa
    "haw",     // Hawaiian
    "he",      // Hebrew
    "hi",      // Hindi
    "hmn",     // Hmong
    "hr",      // Croatian
    "ht",      // Haitian Creole
    "hu",      // Hungarian
    "hy",      // Armenian
    "ia",      // Interlingua
    "id",      // Indonesian
    "ig",      // Igbo
    "is",      // Icelandic
    "it",      // Italian
    "it-CH",   // Italian (Switzerland)
    "it-IT",   // Italian (Italy)
    "ja",      // Japanese
    "jv",      // Javanese
    "ka",      // Georgian
    "kk",      // Kazakh
    "km",      // Cambodian
    "kn",      // Kannada
    "ko",      // Korean
    "ku",      // Kurdish
    "ky",      // Kyrgyz
    "la",      // Latin
    "lb",      // Luxembourgish
    "ln",      // Lingala
    "lo",      // Laothian
    "lt",      // Lithuanian
    "lv",      // Latvian
    "mg",      // Malagasy
    "mi",      // Maori
    "mk",      // Macedonian
    "ml",      // Malayalam
    "mn",      // Mongolian
    "mo",      // Moldavian
    "mr",      // Marathi
    "ms",      // Malay
    "mt",      // Maltese
    "my",      // Burmese
    "nb",      // Norwegian (Bokmal)
    "ne",      // Nepali
    "nl",      // Dutch
    "nn",      // Norwegian (Nynorsk)
    "no",      // Norwegian
    "ny",      // Nyanja
    "oc",      // Occitan
    "om",      // Oromo
    "or",      // Oriya
    "pa",      // Punjabi
    "pl",      // Polish
    "ps",      // Pashto
    "pt",      // Portuguese
    "pt-BR",   // Portuguese (Brazil)
    "pt-PT",   // Portuguese (Portugal)
    "qu",      // Quechua
    "rm",      // Romansh
    "ro",      // Romanian
    "ru",      // Russian
    "sd",      // Sindhi
    "sh",      // Serbo-Croatian
    "si",      // Sinhalese
    "sk",      // Slovak
    "sl",      // Slovenian
    "sm",      // Samoan
    "sn",      // Shona
    "so",      // Somali
    "sq",      // Albanian
    "sr",      // Serbian
    "st",      // Sesotho
    "su",      // Sundanese
    "sv",      // Swedish
    "sw",      // Swahili
    "ta",      // Tamil
    "te",      // Telugu
    "tg",      // Tajik
    "th",      // Thai
    "ti",      // Tigrinya
    "tk",      // Turkmen
    "to",      // Tonga
    "tr",      // Turkish
    "tt",      // Tatar
    "tw",      // Twi
    "ug",      // Uighur
    "uk",      // Ukrainian
    "ur",      // Urdu
    "uz",      // Uzbek
    "vi",      // Vietnamese
    "wa",      // Walloon
    "xh",      // Xhosa
    "yi",      // Yiddish
    "yo",      // Yoruba
    "zh",      // Chinese
    "zh-CN",   // Chinese (China)
    "zh-HK",   // Chinese (Hong Kong)
    "zh-TW",   // Chinese (Taiwan)
    "zu",      // Zulu
};

// Returns true if |locale_name| has an alias in the ICU data file.
bool IsDuplicateName(const std::string& locale_name) {
  static const char* const kDuplicateNames[] = {
    "ar_001",
    "en",
    "en_001",
    "en_150",
    "pt", // pt-BR and pt-PT are used.
    "zh",
    "zh_hans_cn",
    "zh_hant_hk",
    "zh_hant_mo",
    "zh_hans_sg",
    "zh_hant_tw"
  };

  // Skip all the es_Foo other than es_419 for now.
  if (base::StartsWith(locale_name, "es_",
                       base::CompareCase::INSENSITIVE_ASCII))
    return !base::EndsWith(locale_name, "419", base::CompareCase::SENSITIVE);
  for (const char* duplicate_name : kDuplicateNames) {
    if (base::EqualsCaseInsensitiveASCII(duplicate_name, locale_name))
      return true;
  }
  return false;
}

// We added 30+ minimally populated locales with only a few entries
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

#if !defined(OS_MACOSX)
bool IsLocaleAvailable(const std::string& locale) {
  // If locale has any illegal characters in it, we don't want to try to
  // load it because it may be pointing outside the locale data file directory.
  if (!base::i18n::IsFilenameLegal(base::ASCIIToUTF16(locale)))
    return false;

  // IsLocalePartiallyPopulated() can be called here for an early return w/o
  // checking the resource availability below. It'd help when Chrome is run
  // under a system locale Chrome is not localized to (e.g.Farsi on Linux),
  // but it'd slow down the start up time a little bit for locales Chrome is
  // localized to. So, we don't call it here.
  if (!l10n_util::IsLocaleSupportedByOS(locale))
    return false;

  return ui::ResourceBundle::LocaleDataPakExists(locale);
}
#endif

// On Linux, the text layout engine Pango determines paragraph directionality
// by looking at the first strongly-directional character in the text. This
// means text such as "Google Chrome foo bar..." will be layed out LTR even
// if "foo bar" is RTL. So this function prepends the necessary RLM in such
// cases.
void AdjustParagraphDirectionality(base::string16* paragraph) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  if (base::i18n::IsRTL() &&
      base::i18n::StringContainsStrongRTLChars(*paragraph)) {
    paragraph->insert(0, 1,
                      static_cast<base::char16>(base::i18n::kRightToLeftMark));
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
      // Filter out locales for which we have only partially populated data
      // and to which Chrome is not localized.
      if (IsLocalePartiallyPopulated(locale_name))
        continue;
      if (!l10n_util::IsLocaleSupportedByOS(locale_name))
        continue;
      // Normalize underscores to hyphens because that's what our locale files
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (base::LowerCaseEqualsASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (base::LowerCaseEqualsASCII(locale_name, "zh-hant")) {
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

namespace l10n_util {

std::string GetLanguage(const std::string& locale) {
  const std::string::size_type hyphen_pos = locale.find('-');
  return std::string(locale, 0, hyphen_pos);
}

// TODO(jshin): revamp this function completely to use a more sytematic
// and generic locale fallback based on ICU/CLDR.
bool CheckAndResolveLocale(const std::string& locale,
                           std::string* resolved_locale) {
#if defined(OS_MACOSX)
  NOTIMPLEMENTED();
  return false;
#else
  if (IsLocaleAvailable(locale)) {
    *resolved_locale = locale;
    return true;
  }

  // If there's a variant, skip over it so we can try without the region
  // code.  For example, ca_ES@valencia should cause us to try ca@valencia
  // before ca.
  std::string::size_type variant_pos = locale.find('@');
  if (variant_pos != std::string::npos)
    return false;

  // If the locale matches language but not country, use that instead.
  // TODO(jungshik) : Nothing is done about languages that Chrome
  // does not support but available on Windows. We fall
  // back to en-US in GetApplicationLocale so that it's a not critical,
  // but we can do better.
  const std::string lang(GetLanguage(locale));
  if (lang.size() < locale.size()) {
    std::string region(locale, lang.size() + 1);
    std::string tmp_locale(lang);
    // Map es-RR other than es-ES to es-419 (Chrome's Latin American
    // Spanish locale).
    if (base::LowerCaseEqualsASCII(lang, "es") &&
        !base::LowerCaseEqualsASCII(region, "es")) {
      tmp_locale.append("-419");
    } else if (base::LowerCaseEqualsASCII(lang, "pt")) {
      // Map pt-RR other than pt-BR to pt-PT. Note that "pt" by itself maps to
      // pt-BR (logic below).
      tmp_locale.append("-PT");
    } else if (base::LowerCaseEqualsASCII(lang, "zh")) {
      // Map zh-HK and zh-MO to zh-TW. Otherwise, zh-FOO is mapped to zh-CN.
      if (base::LowerCaseEqualsASCII(region, "hk") ||
          base::LowerCaseEqualsASCII(region, "mo")) {  // Macao
        tmp_locale.append("-TW");
      } else {
        tmp_locale.append("-CN");
      }
    } else if (base::LowerCaseEqualsASCII(lang, "en")) {
      // Map Australian, Canadian, Indian, New Zealand and South African
      // English to British English for now.
      // TODO(jungshik): en-CA may have to change sides once
      // we have OS locale separate from app locale (Chrome's UI language).
      if (base::LowerCaseEqualsASCII(region, "au") ||
          base::LowerCaseEqualsASCII(region, "ca") ||
          base::LowerCaseEqualsASCII(region, "in") ||
          base::LowerCaseEqualsASCII(region, "nz") ||
          base::LowerCaseEqualsASCII(region, "za")) {
        tmp_locale.append("-GB");
      } else {
        tmp_locale.append("-US");
      }
    }
    if (IsLocaleAvailable(tmp_locale)) {
      resolved_locale->swap(tmp_locale);
      return true;
    }
  }

  // Google updater uses no, tl, iw and en for our nb, fil, he, and en-US.
  // Note that pt-RR is mapped to pt-PT above, but we want pt -> pt-BR here.
  struct {
    const char* source;
    const char* dest;
  } alias_map[] = {
      {"en", "en-US"}, {"iw", "he"},  {"no", "nb"},
      {"pt", "pt-BR"}, {"tl", "fil"}, {"zh", "zh-CN"},
  };
  for (const auto& alias : alias_map) {
    if (base::LowerCaseEqualsASCII(lang, alias.source)) {
      std::string tmp_locale(alias.dest);
      if (IsLocaleAvailable(tmp_locale)) {
        resolved_locale->swap(tmp_locale);
        return true;
      }
    }
  }

  return false;
#endif
}

std::string GetApplicationLocaleInternal(const std::string& pref_locale) {
#if defined(OS_MACOSX)

  // Use any override (Cocoa for the browser), otherwise use the preference
  // passed to the function.
  std::string app_locale = l10n_util::GetLocaleOverride();
  if (app_locale.empty())
    app_locale = pref_locale;

  // The above should handle all of the cases Chrome normally hits, but for some
  // unit tests, we need something to fall back too.
  if (app_locale.empty())
    app_locale = "en-US";

  return app_locale;

#else

  std::string resolved_locale;
  std::vector<std::string> candidates;

  // We only use --lang and the app pref on Windows.  On Linux, we only
  // look at the LC_*/LANG environment variables.  We do, however, pass --lang
  // to renderer and plugin processes so they know what language the parent
  // process decided to use.

#if defined(OS_WIN)

  // First, try the preference value.
  if (!pref_locale.empty())
    candidates.push_back(base::i18n::GetCanonicalLocale(pref_locale));

  // Next, try the overridden locale.
  const std::vector<std::string>& languages = l10n_util::GetLocaleOverrides();
  if (!languages.empty()) {
    candidates.reserve(candidates.size() + languages.size());
    std::transform(languages.begin(), languages.end(),
                   std::back_inserter(candidates),
                   &base::i18n::GetCanonicalLocale);
  } else {
    // If no override was set, defer to ICU
    candidates.push_back(base::i18n::GetConfiguredLocale());
  }

#elif defined(OS_ANDROID)

  // Try pref_locale first.
  if (!pref_locale.empty())
    candidates.push_back(base::i18n::GetCanonicalLocale(pref_locale));

  // On Android, query java.util.Locale for the default locale.
  candidates.push_back(base::android::GetDefaultLocaleString());

#elif defined(USE_GLIB) && !defined(OS_CHROMEOS)

  // GLib implements correct environment variable parsing with
  // the precedence order: LANGUAGE, LC_ALL, LC_MESSAGES and LANG.
  // We used to use our custom parsing code along with ICU for this purpose.
  // If we have a port that does not depend on GTK, we have to
  // restore our custom code for that port.
  const char* const* languages = g_get_language_names();
  DCHECK(languages);  // A valid pointer is guaranteed.
  DCHECK(*languages);  // At least one entry, "C", is guaranteed.

  for (; *languages != NULL; ++languages) {
    candidates.push_back(base::i18n::GetCanonicalLocale(*languages));
  }

#else

  // By default, use the application locale preference. This applies to ChromeOS
  // and linux systems without glib.
  if (!pref_locale.empty())
    candidates.push_back(pref_locale);

#endif

  std::vector<std::string>::const_iterator i = candidates.begin();
  for (; i != candidates.end(); ++i) {
    if (CheckAndResolveLocale(*i, &resolved_locale)) {
      return resolved_locale;
    }
  }

  // Fallback on en-US.
  const std::string fallback_locale("en-US");
  if (IsLocaleAvailable(fallback_locale)) {
    return fallback_locale;
  }

  return std::string();

#endif
}

std::string GetApplicationLocale(const std::string& pref_locale,
                                 bool set_icu_locale) {
  const std::string locale = GetApplicationLocaleInternal(pref_locale);
  if (set_icu_locale && !locale.empty())
    base::i18n::SetICUDefaultLocale(locale);
  return locale;
}

std::string GetApplicationLocale(const std::string& pref_locale) {
  return GetApplicationLocale(pref_locale, true /* set_icu_locale */);
}

bool IsLocaleNameTranslated(const char* locale,
                            const std::string& display_locale) {
  base::string16 display_name =
      l10n_util::GetDisplayNameForLocale(locale, display_locale, false);
  // Because ICU sets the error code to U_USING_DEFAULT_WARNING whether or not
  // uloc_getDisplayName returns the actual translation or the default
  // value (locale code), we have to rely on this hack to tell whether
  // the translation is available or not.  If ICU doesn't have a translated
  // name for this locale, GetDisplayNameForLocale will just return the
  // locale code.
  return !base::IsStringASCII(display_name) ||
      base::UTF16ToASCII(display_name) != locale;
}

base::string16 GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale,
                                       bool is_for_ui) {
  std::string locale_code = locale;
  // Internally, we use the language code of zh-CN and zh-TW, but we want the
  // display names to be Chinese (Simplified) and Chinese (Traditional) instead
  // of Chinese (China) and Chinese (Taiwan).
  // Translate uses "tl" (Tagalog) to mean "fil" (Filipino) until Google
  // translate is changed to understand "fil". Make "tl" alias to "fil".
  if (locale_code == "zh-CN")
    locale_code = "zh-Hans";
  else if (locale_code == "zh-TW")
    locale_code = "zh-Hant";
  else if (locale_code == "tl")
    locale_code = "fil";
  else if (locale_code == "mo")
    locale_code = "ro-MD";

  base::string16 display_name;
#if defined(OS_IOS)
  // Use the Foundation API to get the localized display name, removing the need
  // for the ICU data file to include this data.
  display_name = GetDisplayNameForLocale(locale_code, display_locale);
#else
#if defined(OS_ANDROID)
  // Use Java API to get locale display name so that we can remove most of
  // the lang data from icu data to reduce binary size, except for zh-Hans and
  // zh-Hant because the current Android Java API doesn't support scripts.
  // TODO(wangxianzhu): remove the special handling of zh-Hans and zh-Hant once
  // Android Java API supports scripts.
  if (!base::StartsWith(locale_code, "zh-Han", base::CompareCase::SENSITIVE)) {
    display_name = GetDisplayNameForLocale(locale_code, display_locale);
  } else
#endif  // defined(OS_ANDROID)
  {
    UErrorCode error = U_ZERO_ERROR;
    const int kBufferSize = 1024;

    int actual_size;
    // For Country code in ICU64 we need to call uloc_getDisplayCountry
    if (locale_code[0] == '-' || locale_code[0] == '_') {
      actual_size = uloc_getDisplayCountry(
          locale_code.c_str(), display_locale.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    } else {
      actual_size = uloc_getDisplayName(
          locale_code.c_str(), display_locale.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    }
    DCHECK(U_SUCCESS(error));
    display_name.resize(actual_size);
  }
#endif

  // Add directional markup so parentheses are properly placed.
  if (is_for_ui && base::i18n::IsRTL())
    base::i18n::AdjustStringForLocaleDirection(&display_name);
  return display_name;
}

base::string16 GetDisplayNameForCountry(const std::string& country_code,
                                        const std::string& display_locale) {
  return GetDisplayNameForLocale("_" + country_code, display_locale, false);
}

std::string NormalizeLocale(const std::string& locale) {
  std::string normalized_locale(locale);
  std::replace(normalized_locale.begin(), normalized_locale.end(), '-', '_');

  return normalized_locale;
}

void GetParentLocales(const std::string& current_locale,
                      std::vector<std::string>* parent_locales) {
  std::string locale(NormalizeLocale(current_locale));

  const int kNameCapacity = 256;
  char parent[kNameCapacity];
  base::strlcpy(parent, locale.c_str(), kNameCapacity);
  parent_locales->push_back(parent);
  UErrorCode err = U_ZERO_ERROR;
  while (uloc_getParent(parent, parent, kNameCapacity, &err) > 0) {
    if (U_FAILURE(err))
      break;
    parent_locales->push_back(parent);
  }
}

bool IsValidLocaleSyntax(const std::string& locale) {
  // Check that the length is plausible.
  if (locale.size() < 2 || locale.size() >= ULOC_FULLNAME_CAPACITY)
    return false;

  // Strip off the part after an '@' sign, which might contain keywords,
  // as in en_IE@currency=IEP or fr@collation=phonebook;calendar=islamic-civil.
  // We don't validate that part much, just check that there's at least one
  // equals sign in a plausible place. Normalize the prefix so that hyphens
  // are changed to underscores.
  std::string prefix = NormalizeLocale(locale);
  size_t split_point = locale.find("@");
  if (split_point != std::string::npos) {
    std::string keywords = locale.substr(split_point + 1);
    prefix = locale.substr(0, split_point);

    size_t equals_loc = keywords.find("=");
    if (equals_loc == std::string::npos ||
        equals_loc < 1 || equals_loc > keywords.size() - 2)
      return false;
  }

  // Check that all characters before the at-sign are alphanumeric or
  // underscore.
  for (size_t i = 0; i < prefix.size(); i++) {
    char ch = prefix[i];
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '_')
      return false;
  }

  // Check that the initial token (before the first hyphen/underscore)
  // is 1 - 3 alphabetical characters (a language tag).
  for (size_t i = 0; i < prefix.size(); i++) {
    char ch = prefix[i];
    if (ch == '_') {
      if (i < 1 || i > 3)
        return false;
      break;
    }
    if (!base::IsAsciiAlpha(ch))
      return false;
  }

  // Check that the all tokens after the initial token are 1 - 8 characters.
  // (Tokenize/StringTokenizer don't work here, they collapse multiple
  // delimiters into one.)
  int token_len = 0;
  int token_index = 0;
  for (size_t i = 0; i < prefix.size(); i++) {
    if (prefix[i] != '_') {
      token_len++;
      continue;
    }

    if (token_index > 0 && (token_len < 1 || token_len > 8)) {
      return false;
    }
    token_index++;
    token_len = 0;
  }
  if (token_index == 0 && (token_len < 1 || token_len > 3)) {
    return false;
  } else if (token_len < 1 || token_len > 8) {
    return false;
  }

  return true;
}

std::string GetStringUTF8(int message_id) {
  return base::UTF16ToUTF8(GetStringUTF16(message_id));
}

base::string16 GetStringUTF16(int message_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  base::string16 str = rb.GetLocalizedString(message_id);
  AdjustParagraphDirectionality(&str);

  return str;
}

base::string16 GetStringFUTF16(int message_id,
                               const std::vector<base::string16>& replacements,
                               std::vector<size_t>* offsets) {
  // TODO(tc): We could save a string copy if we got the raw string as
  // a StringPiece and were able to call ReplaceStringPlaceholders with
  // a StringPiece format string and base::string16 substitution strings.  In
  // practice, the strings should be relatively short.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const base::string16& format_string = rb.GetLocalizedString(message_id);

#ifndef NDEBUG
  // Make sure every replacement string is being used, so we don't just
  // silently fail to insert one. If |offsets| is non-NULL, then don't do this
  // check as the code may simply want to find the placeholders rather than
  // actually replacing them.
  if (!offsets) {
    // $9 is the highest allowed placeholder.
    for (size_t i = 0; i < 9; ++i) {
      bool placeholder_should_exist = replacements.size() > i;

      base::string16 placeholder = base::ASCIIToUTF16("$");
      placeholder += (L'1' + i);
      size_t pos = format_string.find(placeholder);
      if (placeholder_should_exist) {
        DCHECK_NE(std::string::npos, pos) << " Didn't find a " << placeholder
                                          << " placeholder in "
                                          << format_string;
      } else {
        DCHECK_EQ(std::string::npos, pos) << " Unexpectedly found a "
                                          << placeholder << " placeholder in "
                                          << format_string;
      }
    }
  }
#endif

  base::string16 formatted = base::ReplaceStringPlaceholders(
      format_string, replacements, offsets);
  AdjustParagraphDirectionality(&formatted);

  return formatted;
}

std::string GetStringFUTF8(int message_id,
                           const base::string16& a) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a));
}

std::string GetStringFUTF8(int message_id,
                           const base::string16& a,
                           const base::string16& b) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b));
}

std::string GetStringFUTF8(int message_id,
                           const base::string16& a,
                           const base::string16& b,
                           const base::string16& c) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c));
}

std::string GetStringFUTF8(int message_id,
                           const base::string16& a,
                           const base::string16& b,
                           const base::string16& c,
                           const base::string16& d) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c, d));
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a) {
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  return GetStringFUTF16(message_id, replacements, NULL);
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               const base::string16& b) {
  return GetStringFUTF16(message_id, a, b, NULL);
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               const base::string16& b,
                               const base::string16& c) {
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  replacements.push_back(b);
  replacements.push_back(c);
  return GetStringFUTF16(message_id, replacements, NULL);
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               const base::string16& b,
                               const base::string16& c,
                               const base::string16& d) {
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  replacements.push_back(b);
  replacements.push_back(c);
  replacements.push_back(d);
  return GetStringFUTF16(message_id, replacements, NULL);
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               const base::string16& b,
                               const base::string16& c,
                               const base::string16& d,
                               const base::string16& e) {
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  replacements.push_back(b);
  replacements.push_back(c);
  replacements.push_back(d);
  replacements.push_back(e);
  return GetStringFUTF16(message_id, replacements, NULL);
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  base::string16 result = GetStringFUTF16(message_id, replacements, &offsets);
  DCHECK(offsets.size() == 1);
  *offset = offsets[0];
  return result;
}

base::string16 GetStringFUTF16(int message_id,
                               const base::string16& a,
                               const base::string16& b,
                               std::vector<size_t>* offsets) {
  std::vector<base::string16> replacements;
  replacements.push_back(a);
  replacements.push_back(b);
  return GetStringFUTF16(message_id, replacements, offsets);
}

base::string16 GetStringFUTF16Int(int message_id, int a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

base::string16 GetStringFUTF16Int(int message_id, int64_t a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

base::string16 GetPluralStringFUTF16(int message_id, int number) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), number);
}

std::string GetPluralStringFUTF8(int message_id, int number) {
  return base::UTF16ToUTF8(GetPluralStringFUTF16(message_id, number));
}

base::string16 GetSingleOrMultipleStringUTF16(int message_id,
                                               bool is_multiple) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), is_multiple ? "multiple" : "single");
}

void SortStrings16(const std::string& locale,
                   std::vector<base::string16>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

const std::vector<std::string>& GetAvailableLocales() {
  return g_available_locales.Get();
}

void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes) {
  for (const char* accept_language : kAcceptLanguageList) {
    if (!l10n_util::IsLocaleNameTranslated(accept_language, display_locale)) {
      // TODO(jungshik) : Put them at the end of the list with language codes
      // enclosed by brackets instead of skipping.
      continue;
    }
    locale_codes->push_back(accept_language);
  }
}

bool IsLanguageAccepted(const std::string& display_locale,
                        const std::string& locale) {
  for (const char* accept_language : kAcceptLanguageList) {
    if (accept_language == locale &&
        l10n_util::IsLocaleNameTranslated(locale.c_str(), display_locale)) {
      return true;
    }
  }
  return false;
}

int GetLocalizedContentsWidthInPixels(int pixel_resource_id) {
  int width = 0;
  base::StringToInt(l10n_util::GetStringUTF8(pixel_resource_id), &width);
  DCHECK_GT(width, 0);
  return width;
}

const char* const* GetAcceptLanguageListForTesting() {
  return kAcceptLanguageList;
}

size_t GetAcceptLanguageListSizeForTesting() {
  return base::size(kAcceptLanguageList);
}

}  // namespace l10n_util
