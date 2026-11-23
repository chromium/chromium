// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/localized_strings.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/grit/ui_base_test_resources.h"

namespace l10n_util {
namespace {

using LocalizedStringsTest = PlatformTest;

// This test historically only ran on these platforms (it lived inside the
// non-Apple/Android-only part of l10n_util_unittest.cc).
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
TEST_F(LocalizedStringsTest, GetString) {
  std::string s = l10n_util::GetStringUTF8(IDS_SIMPLE);
  EXPECT_EQ(std::string("Hello World!"), s);

  s = l10n_util::GetStringFUTF8(IDS_PLACEHOLDERS, u"chrome", u"10");
  EXPECT_EQ(std::string("Hello, chrome. Your number is 10."), s);

  std::u16string s16 = l10n_util::GetStringFUTF16Int(IDS_PLACEHOLDERS_2, 20);

  // Consecutive '$' characters override any placeholder functionality.
  // See //base/strings/string_util.h ReplaceStringPlaceholders().
  EXPECT_EQ(u"You owe me $$1.", s16);
}

// Reproduces b/585076655: GetStringFUTF16(int, const std::u16string&, size_t*)
// crashes when the localized template has no `$1` placeholder, e.g. when a
// translation dropped the placeholder. `ReplaceStringPlaceholders()` then
// reports no offsets, so the
// `CHECK_EQ(1u, offsets.size(), base::NotFatalUntil::M163)` fails and
// `offsets[0]` is read out of bounds. The read trips the libc++ hardening
// assertion, which is fatal even in builds where DCHECKs are off.
// IDS_SIMPLE ("Hello World!") has no placeholders, so it acts like such a
// template.
//
// TODO(b/585076655): Once GetStringFUTF16() handles a missing placeholder,
// replace this death test with a check of the expected result and offset.
TEST_F(LocalizedStringsTest, GetStringFUTF16WithOffsetMissingPlaceholder) {
  size_t offset = 0;
  EXPECT_DEATH_IF_SUPPORTED(
      l10n_util::GetStringFUTF16(IDS_SIMPLE, u"replacement", &offset), "");
}
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

TEST_F(LocalizedStringsTest, FormatStringComputeCorrectOffsetInRTL) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_EQ(true, base::i18n::IsRTL());
  // Use a format string that contains Strong RTL Chars.
  const std::u16string kFormatString(u"كلمة مرور $1");
  std::vector<size_t> offsets;
  l10n_util::FormatString(kFormatString, {u"Replacement"}, &offsets);
  ASSERT_FALSE(offsets.empty());
  // On Linux, an extra base::i18n::kRightToLeftMark character is appended for
  // the text rendering engine to render the string correctly. This should be
  // considered when computing the offsets.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(offsets[0], 11u);
#else
  EXPECT_EQ(offsets[0], 10u);
#endif
}

}  // namespace
}  // namespace l10n_util
