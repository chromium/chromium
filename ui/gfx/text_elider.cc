// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements utility functions for eliding and formatting UI text.
//
// Note that several of the functions declared in text_elider.h are implemented
// in this file using helper classes in an unnamed namespace.

#include "ui/gfx/text_elider.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/umachine.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_utils.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

namespace gfx {

namespace {

#if BUILDFLAG(IS_IOS)
// The returned string will have at least one character besides the ellipsis
// on either side of '@'; if that's impossible, a single ellipsis is returned.
// If possible, only the username is elided. Otherwise, the domain is elided
// in the middle, splitting available width equally with the elided username.
// If the username is short enough that it doesn't need half the available
// width, the elided domain will occupy that extra width.
std::u16string ElideEmail(const std::u16string& email,
                          const FontList& font_list,
                          float available_pixel_width) {
  if (GetStringWidthF(email, font_list) <= available_pixel_width)
    return email;

  // Split the email into its local-part (username) and domain-part. The email
  // spec allows for @ symbols in the username under some special requirements,
  // but not in the domain part, so splitting at the last @ symbol is safe.
  const size_t split_index = email.find_last_of('@');
  DCHECK_NE(split_index, std::u16string::npos);
  std::u16string username = email.substr(0, split_index);
  std::u16string domain = email.substr(split_index + 1);
  DCHECK(!username.empty());
  DCHECK(!domain.empty());

  // Subtract the @ symbol from the available width as it is mandatory.
  const std::u16string kAtSignUTF16 = u"@";
  available_pixel_width -= GetStringWidthF(kAtSignUTF16, font_list);

  // Check whether eliding the domain is necessary: if eliding the username
  // is sufficient, the domain will not be elided.
  const float full_username_width = GetStringWidthF(username, font_list);
  const float available_domain_width =
      available_pixel_width -
      std::min(full_username_width,
               GetStringWidthF(username.substr(0, 1) + kEllipsisUTF16,
                               font_list));
  if (GetStringWidthF(domain, font_list) > available_domain_width) {
    // Elide the domain so that it only takes half of the available width.
    // Should the username not need all the width available in its half, the
    // domain will occupy the leftover width.
    // If |desired_domain_width| is greater than |available_domain_width|: the
    // minimal username elision allowed by the specifications will not fit; thus
    // |desired_domain_width| must be <= |available_domain_width| at all cost.
    const float desired_domain_width =
        std::min(available_domain_width,
                 std::max(available_pixel_width - full_username_width,
                          available_pixel_width / 2));
    domain = ElideText(domain, font_list, desired_domain_width, ELIDE_MIDDLE);
    // Failing to elide the domain such that at least one character remains
    // (other than the ellipsis itself) remains: return a single ellipsis.
    if (domain.length() <= 1U)
      return std::u16string(kEllipsisUTF16);
  }

  // Fit the username in the remaining width (at this point the elided username
  // is guaranteed to fit with at least one character remaining given all the
  // precautions taken earlier).
  available_pixel_width -= GetStringWidthF(domain, font_list);
  username = ElideText(username, font_list, available_pixel_width, ELIDE_TAIL);
  return username + kAtSignUTF16 + domain;
}
#endif

bool GetDefaultWhitespaceElision(bool elide_in_middle,
                                 bool elide_at_beginning) {
  return elide_at_beginning || !elide_in_middle;
}

}  // namespace

// U+2026 in utf8
const char kEllipsis[] = "\xE2\x80\xA6";
const char16_t kEllipsisUTF16[] = {0x2026, 0};
const char16_t kForwardSlash = '/';

StringSlicer::StringSlicer(const std::u16string& text,
                           const std::u16string& ellipsis,
                           bool elide_in_middle,
                           bool elide_at_beginning,
                           std::optional<bool> elide_whitespace)
    : text_(text),
      ellipsis_(ellipsis),
      elide_in_middle_(elide_in_middle),
      elide_at_beginning_(elide_at_beginning),
      elide_whitespace_(elide_whitespace
                            ? *elide_whitespace
                            : GetDefaultWhitespaceElision(elide_in_middle,
                                                          elide_at_beginning)) {
}

std::u16string StringSlicer::CutString(size_t length,
                                       bool insert_ellipsis) const {
  const std::u16string ellipsis_text =
      insert_ellipsis ? *ellipsis_ : std::u16string();

  // For visual consistency, when eliding at either end of the string, excess
  // space should be trimmed from the text to return "Foo bar..." instead of
  // "Foo bar ...".

  if (elide_at_beginning_) {
    return ellipsis_text +
           text_->substr(FindValidBoundaryAfter(
               *text_, text_->length() - length, elide_whitespace_));
  }

  if (!elide_in_middle_) {
    return text_->substr(
               0, FindValidBoundaryBefore(*text_, length, elide_whitespace_)) +
           ellipsis_text;
  }

  // Put the extra character, if any, before the cut.
  // Extra space around the ellipses will *not* be trimmed for |elide_in_middle|
  // mode (we can change this later). The reason is that when laying out a
  // column of middle-trimmed lines of text (such as a list of paths), the
  // desired appearance is to be fully justified and the elipses should more or
  // less line up; eliminating space would make the text look more ragged.
  const size_t half_length = length / 2;
  const size_t prefix_length =
      FindValidBoundaryBefore(*text_, length - half_length, elide_whitespace_);
  const size_t suffix_start = FindValidBoundaryAfter(
      *text_, text_->length() - half_length, elide_whitespace_);
  return text_->substr(0, prefix_length) + ellipsis_text +
         text_->substr(suffix_start);
}

std::u16string ElideFilename(const base::FilePath& filename,
                             const FontList& font_list,
                             float available_pixel_width) {
#if BUILDFLAG(IS_WIN)
  std::u16string filename_utf16 = WideToUTF16(filename.value());
  std::u16string extension = WideToUTF16(filename.Extension());
  std::u16string rootname =
      WideToUTF16(filename.BaseName().RemoveExtension().value());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::u16string filename_utf16 =
      WideToUTF16(base::SysNativeMBToWide(filename.value()));
  std::u16string extension =
      WideToUTF16(base::SysNativeMBToWide(filename.Extension()));
  std::u16string rootname = WideToUTF16(
      base::SysNativeMBToWide(filename.BaseName().RemoveExtension().value()));
#endif

  const float full_width = GetStringWidthF(filename_utf16, font_list);
  if (full_width <= available_pixel_width)
    return base::i18n::GetDisplayStringInLTRDirectionality(filename_utf16);

  if (rootname.empty() || extension.empty()) {
    const std::u16string elided_name =
        ElideText(filename_utf16, font_list, available_pixel_width, ELIDE_TAIL);
    return base::i18n::GetDisplayStringInLTRDirectionality(elided_name);
  }

  const float ext_width = GetStringWidthF(extension, font_list);
  const float root_width = GetStringWidthF(rootname, font_list);

  // We may have trimmed the path.
  if (root_width + ext_width <= available_pixel_width) {
    const std::u16string elided_name = rootname + extension;
    return base::i18n::GetDisplayStringInLTRDirectionality(elided_name);
  }

  if (ext_width >= available_pixel_width) {
    const std::u16string elided_name = ElideText(
        rootname + extension, font_list, available_pixel_width, ELIDE_MIDDLE);
    return base::i18n::GetDisplayStringInLTRDirectionality(elided_name);
  }

  float available_root_width = available_pixel_width - ext_width;
  std::u16string elided_name =
      ElideText(rootname, font_list, available_root_width, ELIDE_TAIL);
  elided_name += extension;
  return base::i18n::GetDisplayStringInLTRDirectionality(elided_name);
}

std::u16string ElideText(const std::u16string& text,
                         const FontList& font_list,
                         float available_pixel_width,
                         ElideBehavior behavior) {
#if !BUILDFLAG(IS_IOS)
  DCHECK_NE(behavior, FADE_TAIL);
  std::unique_ptr<RenderText> render_text = RenderText::CreateRenderText();
  render_text->SetCursorEnabled(false);
  render_text->SetFontList(font_list);
  available_pixel_width = std::ceil(available_pixel_width);
  render_text->SetDisplayRect(
      gfx::ToEnclosingRect(gfx::RectF(gfx::SizeF(available_pixel_width, 1))));
  render_text->SetElideBehavior(behavior);
  render_text->SetText(text);
  return render_text->GetDisplayText();
#else
  DCHECK_NE(behavior, FADE_TAIL);
  if (text.empty() || behavior == FADE_TAIL || behavior == NO_ELIDE ||
      GetStringWidthF(text, font_list) <= available_pixel_width) {
    return text;
  }
  if (behavior == ELIDE_EMAIL)
    return ElideEmail(text, font_list, available_pixel_width);

  const bool elide_in_middle = (behavior == ELIDE_MIDDLE);
  const bool elide_at_beginning = (behavior == ELIDE_HEAD);
  const bool insert_ellipsis = (behavior != TRUNCATE);
  const std::u16string ellipsis = std::u16string(kEllipsisUTF16);
  StringSlicer slicer(text, ellipsis, elide_in_middle, elide_at_beginning);

  if (insert_ellipsis &&
      GetStringWidthF(ellipsis, font_list) > available_pixel_width)
    return std::u16string();

  // Use binary search to compute the elided text.
  size_t lo = 0;
  size_t hi = text.length() - 1;
  size_t guess;
  std::u16string cut;
  for (guess = std::midpoint(lo, hi); lo <= hi; guess = std::midpoint(lo, hi)) {
    // We check the width of the whole desired string at once to ensure we
    // handle kerning/ligatures/etc. correctly.
    // TODO(skanuj) : Handle directionality of ellipsis based on adjacent
    // characters.  See crbug.com/327963.
    cut = slicer.CutString(guess, insert_ellipsis);
    const float guess_width = GetStringWidthF(cut, font_list);
    if (guess_width == available_pixel_width)
      break;
    if (guess_width > available_pixel_width) {
      hi = guess - 1;
      // Move back on the loop terminating condition when the guess is too wide.
      if (hi < lo)
        lo = hi;
    } else {
      lo = guess + 1;
    }
  }

  return cut;
#endif
}

bool ElideString(const std::u16string& input,
                 size_t max_len,
                 std::u16string* output) {
  if (input.length() <= max_len) {
    output->assign(input);
    return false;
  }

  switch (max_len) {
    case 0:
      output->clear();
      break;
    case 1:
      output->assign(input.substr(0, 1));
      break;
    case 2:
      output->assign(input.substr(0, 2));
      break;
    case 3:
      output->assign(input.substr(0, 1) + u"." +
                     input.substr(input.length() - 1));
      break;
    case 4:
      output->assign(input.substr(0, 1) + u".." +
                     input.substr(input.length() - 1));
      break;
    default: {
      size_t rstr_len = (max_len - 3) / 2;
      size_t lstr_len = rstr_len + ((max_len - 3) % 2);
      output->assign(input.substr(0, lstr_len) + u"..." +
                     input.substr(input.length() - rstr_len));
      break;
    }
  }

  return true;
}

namespace {

// Internal class used to track progress of a rectangular string elide
// operation.  Exists so the top-level ElideRectangleString() function
// can be broken into smaller methods sharing this state.
class RectangleString {
 public:
  RectangleString(size_t max_rows,
                  size_t max_cols,
                  bool strict,
                  std::u16string* output)
      : max_rows_(max_rows),
        max_cols_(max_cols),
        current_row_(0),
        current_col_(0),
        strict_(strict),
        suppressed_(false),
        output_(output) {}

  RectangleString(const RectangleString&) = delete;
  RectangleString& operator=(const RectangleString&) = delete;

  // Perform deferred initializations following creation.  Must be called
  // before any input can be added via AddString().
  void Init() { output_->clear(); }

  // Add an input string, reformatting to fit the desired dimensions.
  // AddString() may be called multiple times to concatenate together
  // multiple strings into the region (the current caller doesn't do
  // this, however).
  void AddString(const std::u16string& input);

  // Perform any deferred output processing.  Must be called after the
  // last AddString() call has occurred.
  bool Finalize();

 private:
  // Add a line to the rectangular region at the current position,
  // either by itself or by breaking it into words.
  void AddLine(const std::u16string& line);

  // Add a word to the rectangular region at the current position,
  // either by itself or by breaking it into characters.
  void AddWord(const std::u16string& word);

  // Add text to the output string if the rectangular boundaries
  // have not been exceeded, advancing the current position.
  void Append(const std::u16string& string);

  // Set the current position to the beginning of the next line.  If
  // |output| is true, add a newline to the output string if the rectangular
  // boundaries have not been exceeded.  If |output| is false, we assume
  // some other mechanism will (likely) do similar breaking after the fact.
  void NewLine(bool output);

  // Maximum number of rows allowed in the output string.
  size_t max_rows_;

  // Maximum number of characters allowed in the output string.
  size_t max_cols_;

  // Current row position, always incremented and may exceed max_rows_
  // when the input can not fit in the region.  We stop appending to
  // the output string, however, when this condition occurs.  In the
  // future, we may want to expose this value to allow the caller to
  // determine how many rows would actually be required to hold the
  // formatted string.
  size_t current_row_;

  // Current character position, should never exceed max_cols_.
  size_t current_col_;

  // True when we do whitespace to newline conversions ourselves.
  bool strict_;

  // True when some of the input has been truncated.
  bool suppressed_;

  // String onto which the output is accumulated.
  raw_ptr<std::u16string> output_;
};

void RectangleString::AddString(const std::u16string& input) {
  base::i18n::BreakIterator lines(input,
                                  base::i18n::BreakIterator::BREAK_NEWLINE);
  if (lines.Init()) {
    while (lines.Advance())
      AddLine(lines.GetString());
  } else {
    NOTREACHED_IN_MIGRATION() << "BreakIterator (lines) init failed";
  }
}

bool RectangleString::Finalize() {
  if (suppressed_) {
    output_->append(u"...");
    return true;
  }
  return false;
}

void RectangleString::AddLine(const std::u16string& line) {
  if (line.length() < max_cols_) {
    Append(line);
  } else {
    base::i18n::BreakIterator words(line,
                                    base::i18n::BreakIterator::BREAK_SPACE);
    if (words.Init()) {
      while (words.Advance())
        AddWord(words.GetString());
    } else {
      NOTREACHED_IN_MIGRATION() << "BreakIterator (words) init failed";
    }
  }
  // Account for naturally-occuring newlines.
  ++current_row_;
  current_col_ = 0;
}

void RectangleString::AddWord(const std::u16string& word) {
  if (word.length() < max_cols_) {
    // Word can be made to fit, no need to fragment it.
    if (current_col_ + word.length() >= max_cols_)
      NewLine(strict_);
    Append(word);
  } else {
    // Word is so big that it must be fragmented.
    size_t array_start = 0;
    int char_start = 0;
    base::i18n::UTF16CharIterator chars(word);
    for (; !chars.end(); chars.Advance()) {
      // When boundary is hit, add as much as will fit on this line.
      if (current_col_ + (chars.char_offset() - char_start) >= max_cols_) {
        Append(word.substr(array_start, chars.array_pos() - array_start));
        NewLine(true);
        array_start = chars.array_pos();
        char_start = chars.char_offset();
      }
    }
    // Add the last remaining fragment, if any.
    if (array_start != chars.array_pos())
      Append(word.substr(array_start, chars.array_pos() - array_start));
  }
}

void RectangleString::Append(const std::u16string& string) {
  if (current_row_ < max_rows_)
    output_->append(string);
  else
    suppressed_ = true;
  current_col_ += string.length();
}

void RectangleString::NewLine(bool output) {
  if (current_row_ < max_rows_) {
    if (output)
      output_->append(u"\n");
  } else {
    suppressed_ = true;
  }
  ++current_row_;
  current_col_ = 0;
}

// Internal class used to track progress of a rectangular text elide
// operation.  Exists so the top-level ElideRectangleText() function
// can be broken into smaller methods sharing this state.
class RectangleText {
 public:
  RectangleText(const FontList& font_list,
                float available_pixel_width,
                int available_pixel_height,
                WordWrapBehavior wrap_behavior,
                std::vector<std::u16string>* lines)
      : font_list_(font_list),
        line_height_(font_list.GetHeight()),
        available_pixel_width_(available_pixel_width),
        available_pixel_height_(available_pixel_height),
        wrap_behavior_(wrap_behavior),
        lines_(lines) {}

  RectangleText(const RectangleText&) = delete;
  RectangleText& operator=(const RectangleText&) = delete;

  // Perform deferred initializations following creation.  Must be called
  // before any input can be added via AddString().
  void Init() { lines_->clear(); }

  // Add an input string, reformatting to fit the desired dimensions.
  // AddString() may be called multiple times to concatenate together
  // multiple strings into the region (the current caller doesn't do
  // this, however).
  void AddString(const std::u16string& input);

  // Perform any deferred output processing.  Must be called after the last
  // AddString() call has occurred. Returns a combination of
  // |ReformattingResultFlags| indicating whether the given width or height was
  // insufficient, leading to elision or truncation.
  int Finalize();

 private:
  // Add a line to the rectangular region at the current position,
  // either by itself or by breaking it into words.
  void AddLine(const std::u16string& line);

  // Wrap the specified word across multiple lines.
  int WrapWord(const std::u16string& word);

  // Add a long word - wrapping, eliding or truncating per the wrap behavior.
  int AddWordOverflow(const std::u16string& word);

  // Add a word to the rectangular region at the current position.
  int AddWord(const std::u16string& word);

  // Append the specified |text| to the current output line, incrementing the
  // running width by the specified amount. This is an optimization over
  // |AddToCurrentLine()| when |text_width| is already known.
  void AddToCurrentLineWithWidth(const std::u16string& text, float text_width);

  // Append the specified |text| to the current output line.
  void AddToCurrentLine(const std::u16string& text);

  // Set the current position to the beginning of the next line.
  bool NewLine();

  // The font list used for measuring text width.
  const raw_ref<const FontList> font_list_;

  // The height of each line of text.
  const int line_height_;

  // The number of pixels of available width in the rectangle.
  const float available_pixel_width_;

  // The number of pixels of available height in the rectangle.
  const int available_pixel_height_;

  // The wrap behavior for words that are too long to fit on a single line.
  const WordWrapBehavior wrap_behavior_;

  // The current running width.
  float current_width_ = 0;

  // The current running height.
  int current_height_ = 0;

  // The current line of text.
  std::u16string current_line_;

  // Indicates whether the last line ended with \n.
  bool last_line_ended_in_lf_ = false;

  // The output vector of lines.
  raw_ptr<std::vector<std::u16string>> lines_;

  // Indicates whether a word was so long that it had to be truncated or elided
  // to fit the available width.
  bool insufficient_width_ = false;

  // Indicates whether there were too many lines for the available height.
  bool insufficient_height_ = false;

  // Indicates whether the very first word was truncated.
  bool first_word_truncated_ = false;
};

void RectangleText::AddString(const std::u16string& input) {
  base::i18n::BreakIterator lines(input,
                                  base::i18n::BreakIterator::BREAK_NEWLINE);
  if (lines.Init()) {
    while (!insufficient_height_ && lines.Advance()) {
      std::u16string line = lines.GetString();
      // The BREAK_NEWLINE iterator will keep the trailing newline character,
      // except in the case of the last line, which may not have one.  Remove
      // the newline character, if it exists.
      last_line_ended_in_lf_ = !line.empty() && line.back() == '\n';
      if (last_line_ended_in_lf_)
        line.resize(line.length() - 1);
      AddLine(line);
    }
  } else {
    NOTREACHED_IN_MIGRATION() << "BreakIterator (lines) init failed";
  }
}

int RectangleText::Finalize() {
  // Remove trailing whitespace from the last line or remove the last line
  // completely, if it's just whitespace.
  if (!insufficient_height_ && !lines_->empty()) {
    base::TrimWhitespace(lines_->back(), base::TRIM_TRAILING, &lines_->back());
    if (lines_->back().empty() && !last_line_ended_in_lf_)
      lines_->pop_back();
  }
  if (last_line_ended_in_lf_)
    lines_->push_back(std::u16string());
  return (insufficient_width_ ? INSUFFICIENT_SPACE_HORIZONTAL : 0) |
         (insufficient_height_ ? INSUFFICIENT_SPACE_VERTICAL : 0) |
         (first_word_truncated_ ? INSUFFICIENT_SPACE_FOR_FIRST_WORD : 0);
}

void RectangleText::AddLine(const std::u16string& line) {
  const float line_width = GetStringWidthF(line, *font_list_);
  if (line_width <= available_pixel_width_) {
    AddToCurrentLineWithWidth(line, line_width);
  } else {
    // Iterate over positions that are valid to break the line at. In general,
    // these are word boundaries but after any punctuation following the word.
    base::i18n::BreakIterator words(line,
                                    base::i18n::BreakIterator::BREAK_LINE);
    if (words.Init()) {
      while (words.Advance()) {
        const bool truncate = !current_line_.empty();
        const std::u16string& word = words.GetString();
        const int lines_added = AddWord(word);
        if (lines_added) {
          if (truncate) {
            // Trim trailing whitespace from the line that was added.
            const size_t new_line = lines_->size() - lines_added;
            base::TrimWhitespace(lines_->at(new_line), base::TRIM_TRAILING,
                                 &lines_->at(new_line));
          }
          if (base::ContainsOnlyChars(word, base::kWhitespaceUTF16)) {
            // Skip the first space if the previous line was carried over.
            current_width_ = 0;
            current_line_.clear();
          }
        }
      }
    } else {
      NOTREACHED_IN_MIGRATION() << "BreakIterator (words) init failed";
    }
  }
  // Account for naturally-occuring newlines.
  NewLine();
}

int RectangleText::WrapWord(const std::u16string& word) {
  // Word is so wide that it must be fragmented.
  std::u16string text = word;
  int lines_added = 0;
  bool first_fragment = true;
  while (!insufficient_height_ && !text.empty()) {
    std::u16string fragment =
        ElideText(text, *font_list_, available_pixel_width_, TRUNCATE);
    // At least one character has to be added at every line, even if the
    // available space is too small.
    if (fragment.empty())
      fragment = text.substr(0, 1);
    if (!first_fragment && NewLine())
      lines_added++;
    AddToCurrentLine(fragment);
    text = text.substr(fragment.length());
    first_fragment = false;
  }
  return lines_added;
}

int RectangleText::AddWordOverflow(const std::u16string& word) {
  int lines_added = 0;

  // Unless this is the very first word, put it on a new line.
  if (!current_line_.empty()) {
    if (!NewLine())
      return 0;
    lines_added++;
  } else if (lines_->empty()) {
    first_word_truncated_ = true;
  }

  if (wrap_behavior_ == IGNORE_LONG_WORDS) {
    current_line_ = word;
    current_width_ = available_pixel_width_;
  } else if (wrap_behavior_ == WRAP_LONG_WORDS) {
    lines_added += WrapWord(word);
  } else {
    const ElideBehavior elide_behavior =
        (wrap_behavior_ == ELIDE_LONG_WORDS ? ELIDE_TAIL : TRUNCATE);
    const std::u16string elided_word =
        ElideText(word, *font_list_, available_pixel_width_, elide_behavior);
    AddToCurrentLine(elided_word);
    insufficient_width_ = true;
  }

  return lines_added;
}

int RectangleText::AddWord(const std::u16string& word) {
  int lines_added = 0;
  std::u16string trimmed;
  base::TrimWhitespace(word, base::TRIM_TRAILING, &trimmed);
  const float trimmed_width = GetStringWidthF(trimmed, *font_list_);
  if (trimmed_width <= available_pixel_width_) {
    // Word can be made to fit, no need to fragment it.
    if ((current_width_ + trimmed_width > available_pixel_width_) && NewLine())
      lines_added++;
    // Append the non-trimmed word, in case more words are added after.
    AddToCurrentLine(word);
  } else {
    lines_added = AddWordOverflow(wrap_behavior_ == IGNORE_LONG_WORDS ?
                                  trimmed : word);
  }
  return lines_added;
}

void RectangleText::AddToCurrentLine(const std::u16string& text) {
  AddToCurrentLineWithWidth(text, GetStringWidthF(text, *font_list_));
}

void RectangleText::AddToCurrentLineWithWidth(const std::u16string& text,
                                              float text_width) {
  if (current_height_ >= available_pixel_height_) {
    insufficient_height_ = true;
    return;
  }
  current_line_.append(text);
  current_width_ += text_width;
}

bool RectangleText::NewLine() {
  bool line_added = false;
  if (current_height_ < available_pixel_height_) {
    lines_->push_back(current_line_);
    current_line_.clear();
    line_added = true;
  } else {
    insufficient_height_ = true;
  }
  current_height_ += line_height_;
  current_width_ = 0;
  return line_added;
}

}  // namespace

bool ElideRectangleString(const std::u16string& input,
                          size_t max_rows,
                          size_t max_cols,
                          bool strict,
                          std::u16string* output) {
  RectangleString rect(max_rows, max_cols, strict, output);
  rect.Init();
  rect.AddString(input);
  return rect.Finalize();
}

int ElideRectangleText(const std::u16string& input,
                       const FontList& font_list,
                       float available_pixel_width,
                       int available_pixel_height,
                       WordWrapBehavior wrap_behavior,
                       std::vector<std::u16string>* lines) {
  RectangleText rect(font_list, available_pixel_width, available_pixel_height,
                     wrap_behavior, lines);
  rect.Init();
  rect.AddString(input);
  return rect.Finalize();
}

std::u16string TruncateString(const std::u16string& string,
                              size_t length,
                              BreakType break_type) {
  const bool word_break = break_type == WORD_BREAK;
  DCHECK(word_break || (break_type == CHARACTER_BREAK));

  if (string.size() <= length)
    return string;  // No need to elide.

  if (length == 0)
    return std::u16string();  // No room for anything, even an ellipsis.

  // Added to the end of strings that are too big.
  static const char16_t kElideString[] = {0x2026, 0};

  if (length == 1)
    return kElideString;  // Only room for an ellipsis.

  int32_t index = static_cast<int32_t>(length - 1);
  if (word_break) {
    // Use a word iterator to find the first boundary.
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> bi(
        icu::RuleBasedBreakIterator::createWordInstance(
            icu::Locale::getDefault(), status));
    if (U_FAILURE(status))
      return string.substr(0, length - 1) + kElideString;
    icu::UnicodeString bi_text(string.c_str());
    bi->setText(bi_text);
    index = bi->preceding(static_cast<int32_t>(length));
    if (index == icu::BreakIterator::DONE || index == 0) {
      // We either found no valid word break at all, or one right at the
      // beginning of the string. Go back to the end; we'll have to break in the
      // middle of a word.
      index = static_cast<int32_t>(length - 1);
    }
  }

  // By this point, |index| should point at the character that's a candidate for
  // replacing with an ellipsis.  Use a character iterator to check previous
  // characters and stop as soon as we find a previous non-whitespace character.
  icu::StringCharacterIterator char_iterator(string.c_str());
  char_iterator.setIndex(index);
  while (char_iterator.hasPrevious()) {
    char_iterator.previous();
    if (!(u_isspace(char_iterator.current()) ||
          u_charType(char_iterator.current()) == U_CONTROL_CHAR ||
          u_charType(char_iterator.current()) == U_NON_SPACING_MARK)) {
      // Not a whitespace character.  Truncate to everything up to and including
      // this character, and append an ellipsis.
      char_iterator.next();
      return string.substr(0, char_iterator.getIndex()) + kElideString;
    }
  }

  // Couldn't find a previous non-whitespace character.  If we were originally
  // word-breaking, and index != length - 1, then the string is |index|
  // whitespace characters followed by a word we're trying to break in the midst
  // of, and we can fit at least one character of the word in the elided string.
  // Do that rather than just returning an ellipsis.
  if (word_break && (index != static_cast<int32_t>(length - 1)))
    return string.substr(0, length - 1) + kElideString;

  // Trying to break after only whitespace, elide all of it.
  return kElideString;
}

}  // namespace gfx
