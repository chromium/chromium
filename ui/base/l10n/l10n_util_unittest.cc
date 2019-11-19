// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/grit/ui_base_test_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <cstdlib>
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace {

class StringWrapper {
 public:
  explicit StringWrapper(const base::string16& string) : string_(string) {}
  const base::string16& string() const { return string_; }

 private:
  base::string16 string_;

  DISALLOW_COPY_AND_ASSIGN(StringWrapper);
};

}  // namespace

class L10nUtilTest : public PlatformTest {
};

TEST_F(L10nUtilTest, GetString) {
  std::string s = l10n_util::GetStringUTF8(IDS_SIMPLE);
  EXPECT_EQ(std::string("Hello World!"), s);

  s = l10n_util::GetStringFUTF8(IDS_PLACEHOLDERS,
                                UTF8ToUTF16("chrome"),
                                UTF8ToUTF16("10"));
  EXPECT_EQ(std::string("Hello, chrome. Your number is 10."), s);

  base::string16 s16 = l10n_util::GetStringFUTF16Int(IDS_PLACEHOLDERS_2, 20);

  // Consecutive '$' characters override any placeholder functionality.
  // See //base/strings/string_util.h ReplaceStringPlaceholders().
  EXPECT_EQ(UTF8ToUTF16("You owe me $$1."), s16);
}

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// On Mac, we are disabling this test because GetApplicationLocale() as an
// API isn't something that we'll easily be able to unit test in this manner.
// The meaning of that API, on the Mac, is "the locale used by Cocoa's main
// nib file", which clearly can't be stubbed by a test app that doesn't use
// Cocoa.

// On Android, we are disabling this test since GetApplicationLocale() just
// returns the system's locale, which, similarly, is not easily unit tested.

#if defined(OS_POSIX) && defined(USE_GLIB) && !defined(OS_CHROMEOS)
const bool kPlatformHasDefaultLocale = 1;
const bool kUseLocaleFromEnvironment = 1;
const bool kSupportsLocalePreference = 0;
#elif defined(OS_WIN)
const bool kPlatformHasDefaultLocale = 1;
const bool kUseLocaleFromEnvironment = 0;
const bool kSupportsLocalePreference = 1;
#else
const bool kPlatformHasDefaultLocale = 0;
const bool kUseLocaleFromEnvironment = 0;
const bool kSupportsLocalePreference = 1;
#endif

void SetDefaultLocaleForTest(const std::string& tag, base::Environment* env) {
  if (kUseLocaleFromEnvironment)
    env->SetVar("LANGUAGE", tag);
  else
    base::i18n::SetICUDefaultLocale(tag);
}

TEST_F(L10nUtilTest, GetAppLocale) {
  std::unique_ptr<base::Environment> env;
  // Use a temporary locale dir so we don't have to actually build the locale
  // pak files for this test.
  base::ScopedPathOverride locale_dir_override(ui::DIR_LOCALES);
  base::FilePath new_locale_dir;
  ASSERT_TRUE(base::PathService::Get(ui::DIR_LOCALES, &new_locale_dir));
  // Make fake locale files.
  std::string filenames[] = {
      "am", "ca", "ca@valencia", "en-GB", "en-US", "es",    "es-419", "fil",
      "fr", "he", "nb",          "pt-BR", "pt-PT", "zh-CN", "zh-TW",
  };

  for (size_t i = 0; i < base::size(filenames); ++i) {
    base::FilePath filename = new_locale_dir.AppendASCII(
        filenames[i] + ".pak");
    base::WriteFile(filename, "", 0);
  }

  // Keep a copy of ICU's default locale before we overwrite it.
  const std::string original_locale = base::i18n::GetConfiguredLocale();

  if (kPlatformHasDefaultLocale && kUseLocaleFromEnvironment) {
    env = base::Environment::Create();

    // Test the support of LANGUAGE environment variable.
    base::i18n::SetICUDefaultLocale("en-US");
    env->SetVar("LANGUAGE", "xx:fr_CA");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());

    env->SetVar("LANGUAGE", "xx:yy:en_gb.utf-8@quot");
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    env->SetVar("LANGUAGE", "xx:zh-hk");
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
    // when LANGUAGE is specified. If no language specified in LANGUAGE is
    // valid,
    // then just fallback to the default language, which is en-US for us.
    base::i18n::SetICUDefaultLocale("fr-FR");
    env->SetVar("LANGUAGE", "xx:yy");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    env->SetVar("LANGUAGE", "/fr:zh_CN");
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    // Test prioritization of the different environment variables.
    env->SetVar("LANGUAGE", "fr");
    env->SetVar("LC_ALL", "es");
    env->SetVar("LC_MESSAGES", "he");
    env->SetVar("LANG", "nb");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
    env->UnSetVar("LANGUAGE");
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());
    env->UnSetVar("LC_ALL");
    EXPECT_EQ("he", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("he", icu::Locale::getDefault().getLanguage());
    env->UnSetVar("LC_MESSAGES");
    EXPECT_EQ("nb", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("nb", icu::Locale::getDefault().getLanguage());
    env->UnSetVar("LANG");

    SetDefaultLocaleForTest("ca", env.get());
    EXPECT_EQ("ca", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("ca", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("ca-ES", env.get());
    EXPECT_EQ("ca", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("ca", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("ca@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("ca", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("ca_ES@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("ca", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("ca_ES.UTF8@valencia", env.get());
    EXPECT_EQ("ca@valencia", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("ca", icu::Locale::getDefault().getLanguage());
  }

  SetDefaultLocaleForTest("en-US", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

  SetDefaultLocaleForTest("xx", env.get());
  EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string()));
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

  if (!kPlatformHasDefaultLocale) {
    // ChromeOS & embedded use only browser prefs in GetApplicationLocale(),
    // ignoring the environment, and default to en-US. Other platforms honor
    // the default locale from the OS or environment.
    SetDefaultLocaleForTest("en-GB", env.get());
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(""));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-GB"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-AU"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-NZ"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-CA"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-US", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale("en-ZA"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  } else {
    // Most platforms have an OS-provided locale. This locale is preferred.
    SetDefaultLocaleForTest("en-GB", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("fr-CA", env.get());
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("es-MX", env.get());
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("es-AR", env.get());
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("es-ES", env.get());
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("es", env.get());
    EXPECT_EQ("es", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("pt-PT", env.get());
    EXPECT_EQ("pt-PT", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("pt", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("pt-BR", env.get());
    EXPECT_EQ("pt-BR", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("pt", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("pt-AO", env.get());
    EXPECT_EQ("pt-PT", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("pt", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("pt", env.get());
    EXPECT_EQ("pt-BR", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("pt", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("zh-HK", env.get());
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("zh-MO", env.get());
    EXPECT_EQ("zh-TW", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("zh-SG", env.get());
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("zh", env.get());
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-CA", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-AU", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-NZ", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    SetDefaultLocaleForTest("en-ZA", env.get());
    EXPECT_EQ("en-GB", l10n_util::GetApplicationLocale(std::string()));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  }

  SetDefaultLocaleForTest("en-US", env.get());

  if (kSupportsLocalePreference) {
    // On windows, the user can override the locale in preferences.
    base::i18n::SetICUDefaultLocale("en-US");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr"));
    EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr-CA"));
    EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("en-US");
    // Aliases iw, no, tl to he, nb, fil.
    EXPECT_EQ("he", l10n_util::GetApplicationLocale("iw"));
    EXPECT_STREQ("he", icu::Locale::getDefault().getLanguage());
    EXPECT_EQ("nb", l10n_util::GetApplicationLocale("no"));
    EXPECT_STREQ("nb", icu::Locale::getDefault().getLanguage());
    EXPECT_EQ("fil", l10n_util::GetApplicationLocale("tl"));
    EXPECT_STREQ("fil", icu::Locale::getDefault().getLanguage());
    // es-419 and es-XX (where XX is not Spain) should be
    // mapped to es-419 (Latin American Spanish).
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-419"));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());
    EXPECT_EQ("es", l10n_util::GetApplicationLocale("es-ES"));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());
    EXPECT_EQ("es-419", l10n_util::GetApplicationLocale("es-AR"));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("es-AR");
    EXPECT_EQ("es", l10n_util::GetApplicationLocale("es"));
    EXPECT_STREQ("es", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("zh-HK");
    EXPECT_EQ("zh-CN", l10n_util::GetApplicationLocale("zh-CN"));
    EXPECT_STREQ("zh", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("he");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en"));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("he");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en", false));
    EXPECT_STREQ("he", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("xx", false));
    EXPECT_STREQ("de", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("fr", l10n_util::GetApplicationLocale("fr", false));
    EXPECT_STREQ("de", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en", false));
    EXPECT_STREQ("de", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale("en-US", true));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  } else {
    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string(), false));
    EXPECT_STREQ("de", icu::Locale::getDefault().getLanguage());

    base::i18n::SetICUDefaultLocale("de");
    EXPECT_EQ("en-US", l10n_util::GetApplicationLocale(std::string(), true));
    EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  }

#if defined(OS_WIN)
  base::i18n::SetICUDefaultLocale("am");
  EXPECT_EQ("am", l10n_util::GetApplicationLocale(""));
  EXPECT_STREQ("am", icu::Locale::getDefault().getLanguage());
  base::i18n::SetICUDefaultLocale("en-GB");
  EXPECT_EQ("am", l10n_util::GetApplicationLocale("am"));
  EXPECT_STREQ("am", icu::Locale::getDefault().getLanguage());
#endif  // defined(OS_WIN)

  // Clean up.
  base::i18n::SetICUDefaultLocale(original_locale);
}
#endif  // !defined(OS_MACOSX)

TEST_F(L10nUtilTest, SortStringsUsingFunction) {
  std::vector<std::unique_ptr<StringWrapper>> strings;
  strings.push_back(std::make_unique<StringWrapper>(UTF8ToUTF16("C")));
  strings.push_back(std::make_unique<StringWrapper>(UTF8ToUTF16("d")));
  strings.push_back(std::make_unique<StringWrapper>(UTF8ToUTF16("b")));
  strings.push_back(std::make_unique<StringWrapper>(UTF8ToUTF16("a")));
  l10n_util::SortStringsUsingMethod("en-US",
                                    &strings,
                                    &StringWrapper::string);
  ASSERT_TRUE(UTF8ToUTF16("a") == strings[0]->string());
  ASSERT_TRUE(UTF8ToUTF16("b") == strings[1]->string());
  ASSERT_TRUE(UTF8ToUTF16("C") == strings[2]->string());
  ASSERT_TRUE(UTF8ToUTF16("d") == strings[3]->string());
}

/**
 * Helper method for validating strings that require direcitonal markup.
 * Checks that parentheses are enclosed in appropriate direcitonal markers.
 */
void CheckUiDisplayNameForLocale(const std::string& locale,
                                 const std::string& display_locale,
                                 bool is_rtl) {
  EXPECT_EQ(true, base::i18n::IsRTL());
  base::string16 result = l10n_util::GetDisplayNameForLocale(locale,
                                                       display_locale,
                                                       /* is_for_ui */ true);

  bool rtl_direction = true;
  for (size_t i = 0; i < result.length() - 1; i++) {
    base::char16 ch = result.at(i);
    switch (ch) {
    case base::i18n::kLeftToRightMark:
    case base::i18n::kLeftToRightEmbeddingMark:
      rtl_direction = false;
      break;
    case base::i18n::kRightToLeftMark:
    case base::i18n::kRightToLeftEmbeddingMark:
      rtl_direction = true;
      break;
    case '(':
    case ')':
      EXPECT_EQ(is_rtl, rtl_direction);
    }
  }
}

TEST_F(L10nUtilTest, GetDisplayNameForLocale) {
  // TODO(jungshik): Make this test more extensive.
  // Test zh-CN and zh-TW are treated as zh-Hans and zh-Hant.
  // Displays as "Chinese, Simplified" on iOS 13+ and as "Chinese (Simplified)"
  // on other platforms.
  base::string16 result =
      l10n_util::GetDisplayNameForLocale("zh-CN", "en", false);
  EXPECT_TRUE(
      base::MatchPattern(base::UTF16ToUTF8(result), "Chinese*Simplified*"));

  // Displays as "Chinese, Traditional" on iOS 13+ and as
  // "Chinese (Traditional)" on other platforms.
  result = l10n_util::GetDisplayNameForLocale("zh-TW", "en", false);
  EXPECT_TRUE(
      base::MatchPattern(base::UTF16ToUTF8(result), "Chinese*Traditional*"));

  // tl and fil are not identical to be strict, but we treat them as
  // synonyms.
  result = l10n_util::GetDisplayNameForLocale("tl", "en", false);
  EXPECT_EQ(l10n_util::GetDisplayNameForLocale("fil", "en", false), result);

  result = l10n_util::GetDisplayNameForLocale("pt-BR", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Portuguese (Brazil)"), result);

  result = l10n_util::GetDisplayNameForLocale("es-419", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Spanish (Latin America)"), result);

  result = l10n_util::GetDisplayNameForLocale("mo", "en", false);
  EXPECT_EQ(l10n_util::GetDisplayNameForLocale("ro-MD", "en", false), result);

  result = l10n_util::GetDisplayNameForLocale("-BR", "en", false);
  EXPECT_EQ(ASCIIToUTF16("Brazil"), result);

  result = l10n_util::GetDisplayNameForLocale("xyz-xyz", "en", false);
  EXPECT_EQ(ASCIIToUTF16("xyz (XYZ)"), result);

  // Make sure that en-GB locale has the corect display names.
  result = l10n_util::GetDisplayNameForLocale("en", "en-GB", false);
  EXPECT_EQ(ASCIIToUTF16("English"), result);
  result = l10n_util::GetDisplayNameForLocale("es-419", "en-GB", false);
  EXPECT_EQ(ASCIIToUTF16("Spanish (Latin America)"), result);

  // Check for directional markers when using RTL languages to ensure that
  // direction neutral characters such as parentheses are properly formatted.

  // Keep a copy of ICU's default locale before we overwrite it.
  const std::string original_locale = base::i18n::GetConfiguredLocale();

  base::i18n::SetICUDefaultLocale("he");
  CheckUiDisplayNameForLocale("en-US", "en", false);
  CheckUiDisplayNameForLocale("en-US", "he", true);

  // Clean up.
  base::i18n::SetICUDefaultLocale(original_locale);

  // ToUpper and ToLower should work with embedded NULLs.
  const size_t length_with_null = 4;
  base::char16 buf_with_null[length_with_null] = { 0, 'a', 0, 'b' };
  base::string16 string16_with_null(buf_with_null, length_with_null);

  base::string16 upper_with_null = base::i18n::ToUpper(string16_with_null);
  ASSERT_EQ(length_with_null, upper_with_null.size());
  EXPECT_TRUE(upper_with_null[0] == 0 && upper_with_null[1] == 'A' &&
              upper_with_null[2] == 0 && upper_with_null[3] == 'B');

  base::string16 lower_with_null = base::i18n::ToLower(upper_with_null);
  ASSERT_EQ(length_with_null, upper_with_null.size());
  EXPECT_TRUE(lower_with_null[0] == 0 && lower_with_null[1] == 'a' &&
              lower_with_null[2] == 0 && lower_with_null[3] == 'b');
}

TEST_F(L10nUtilTest, GetDisplayNameForCountry) {
  base::string16 result = l10n_util::GetDisplayNameForCountry("BR", "en");
  EXPECT_EQ(ASCIIToUTF16("Brazil"), result);

  result = l10n_util::GetDisplayNameForCountry("419", "en");
  EXPECT_EQ(ASCIIToUTF16("Latin America"), result);

  result = l10n_util::GetDisplayNameForCountry("xyz", "en");
  EXPECT_EQ(ASCIIToUTF16("XYZ"), result);
}

TEST_F(L10nUtilTest, GetParentLocales) {
  std::vector<std::string> locales;
  const std::string top_locale("sr_Cyrl_RS");
  l10n_util::GetParentLocales(top_locale, &locales);

  ASSERT_EQ(3U, locales.size());
  EXPECT_EQ("sr_Cyrl_RS", locales[0]);
  EXPECT_EQ("sr_Cyrl", locales[1]);
  EXPECT_EQ("sr", locales[2]);
}

TEST_F(L10nUtilTest, IsValidLocaleSyntax) {
  // Test valid locales.
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("de"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("pt"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fil"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("haw"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en-US"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_US"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_GB"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("pt-BR"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_CN"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hans"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hans_CN"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hant"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zh_Hant_TW"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr_CA"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("i-klingon"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("es-419"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE_PREEURO"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE_u_cu_IEP"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("en_IE@currency=IEP"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("fr@x=y"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax("zn_CN@foo=bar"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(
      "fr@collation=phonebook;calendar=islamic-civil"));
  EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(
      "sr_Latn_RS_REVISED@currency=USD"));

  // Test invalid locales.
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax(std::string()));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("x"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("12"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("456"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("a1"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("enUS"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("zhcn"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en.US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en#US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("-en-US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US-"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("123-en-US"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("Latin"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("German"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("pt--BR"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("sl-macedonia"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("@"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@x"));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@x="));
  EXPECT_FALSE(l10n_util::IsValidLocaleSyntax("en-US@=y"));
}

TEST_F(L10nUtilTest, TimeDurationFormatAllLocales) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;

  // Verify that base::TimeDurationFormat() works for all available locales:
  // http://crbug.com/707515
  base::TimeDelta kDelta = base::TimeDelta::FromMinutes(15 * 60 + 42);
  for (const std::string& locale : l10n_util::GetAvailableLocales()) {
    base::i18n::SetICUDefaultLocale(locale);
    base::string16 str;
    const bool result =
        base::TimeDurationFormat(kDelta, base::DURATION_WIDTH_NUMERIC, &str);
    EXPECT_TRUE(result) << "Failed to format duration for " << locale;
    if (result)
      EXPECT_FALSE(str.empty()) << "Got empty string for " << locale;
  }
}
