// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/localized_strings.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"

namespace l10n_util {
namespace {

// On Linux, the text layout engine Pango determines paragraph directionality
// by looking at the first strongly-directional character in the text. This
// means text such as "Google Chrome foo bar..." will be laid out LTR even
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

}  // namespace

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
  // If there is at least one replacement, the formatted string must not be
  // empty.
  CHECK(replacements.empty() || !formatted.empty());
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
  CHECK(offset, base::NotFatalUntil::M163);
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements = {a};
  std::u16string result = GetStringFUTF16(message_id, replacements, &offsets);
  CHECK_EQ(1u, offsets.size(), base::NotFatalUntil::M163);
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

}  // namespace l10n_util
