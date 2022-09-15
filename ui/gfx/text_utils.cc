// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#include <stdint.h>

#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

using base::i18n::UTF16CharIterator;

namespace {

constexpr char16_t kAcceleratorChar = '&';
constexpr char16_t kOpenParenthesisChar = '(';
constexpr char16_t kCloseParenthesisChar = ')';

// Returns true if the specified character must be elided from a string.
// Examples are combining marks and whitespace.
bool IsCombiningMark(UChar32 c) {
  const int8_t char_type = u_charType(c);
  return char_type == U_NON_SPACING_MARK || char_type == U_ENCLOSING_MARK ||
         char_type == U_COMBINING_SPACING_MARK;
}

bool IsSpace(UChar32 c) {
  // Ignore NUL character.
  if (!c)
    return false;
  const int8_t char_type = u_charType(c);
  return char_type == U_SPACE_SEPARATOR || char_type == U_LINE_SEPARATOR ||
         char_type == U_PARAGRAPH_SEPARATOR || char_type == U_CONTROL_CHAR;
}

std::u16string RemoveAcceleratorChar(bool full_removal,
                                     const std::u16string& s,
                                     int* accelerated_char_pos,
                                     int* accelerated_char_span) {
  bool escaped = false;
  ptrdiff_t last_char_pos = -1;
  int last_char_span = 0;
  UTF16CharIterator chars(s);
  std::u16string accelerator_removed;

  // The states of a state machine looking for a CJK-style accelerator (i.e.
  // "(&x)"). |cjk_state| proceeds up from |kFoundNothing| through these states,
  // resetting either when it sees a complete accelerator, or gives up because
  // the current character doesn't match.
  enum {
    kFoundNothing,
    kFoundOpenParen,
    kFoundAcceleratorChar,
    kFoundAccelerator
  } cjk_state = kFoundNothing;
  size_t pre_cjk_size = 0;

  accelerator_removed.reserve(s.size());
  while (!chars.end()) {
    int32_t c = chars.get();
    int array_pos = chars.array_pos();
    chars.Advance();

    if (full_removal) {
      if (cjk_state == kFoundNothing && c == kOpenParenthesisChar) {
        pre_cjk_size = array_pos;
        cjk_state = kFoundOpenParen;
      } else if (cjk_state == kFoundOpenParen && c == kAcceleratorChar) {
        cjk_state = kFoundAcceleratorChar;
      } else if (cjk_state == kFoundAcceleratorChar) {
        // Accept any character as the accelerator.
        cjk_state = kFoundAccelerator;
      } else if (cjk_state == kFoundAccelerator && c == kCloseParenthesisChar) {
        cjk_state = kFoundNothing;
        accelerator_removed.resize(pre_cjk_size);
        pre_cjk_size = 0;
        escaped = false;
        continue;
      } else {
        cjk_state = kFoundNothing;
      }
    }

    if (c != kAcceleratorChar || escaped) {
      int span = chars.array_pos() - array_pos;
      if (escaped && c != kAcceleratorChar) {
        last_char_pos = accelerator_removed.size();
        last_char_span = span;
      }
      for (int i = 0; i < span; i++)
        accelerator_removed.push_back(s[array_pos + i]);
      escaped = false;
    } else {
      escaped = true;
    }
  }

  if (accelerated_char_pos && !full_removal)
    *accelerated_char_pos = last_char_pos;
  if (accelerated_char_span && !full_removal)
    *accelerated_char_span = last_char_span;

  return accelerator_removed;
}

}  // namespace

std::u16string LocateAndRemoveAcceleratorChar(const std::u16string& s,
                                              int* accelerated_char_pos,
                                              int* accelerated_char_span) {
  return RemoveAcceleratorChar(false, s, accelerated_char_pos,
                               accelerated_char_span);
}

std::u16string RemoveAccelerator(const std::u16string& s) {
  return RemoveAcceleratorChar(true, s, nullptr, nullptr);
}

size_t FindValidBoundaryBefore(const std::u16string& text,
                               size_t index,
                               bool trim_whitespace) {
  UTF16CharIterator it = UTF16CharIterator::LowerBound(text, index);

  // First, move left until we're positioned on a code point that is not a
  // combining mark.
  while (!it.start() && IsCombiningMark(it.get()))
    it.Rewind();

  // Next, maybe trim whitespace to the left of the current position.
  if (trim_whitespace) {
    while (!it.start() && IsSpace(it.PreviousCodePoint()))
      it.Rewind();
  }

  return it.array_pos();
}

size_t FindValidBoundaryAfter(const std::u16string& text,
                              size_t index,
                              bool trim_whitespace) {
  UTF16CharIterator it = UTF16CharIterator::UpperBound(text, index);

  // First, move right until we're positioned on a code point that is not a
  // combining mark.
  while (!it.end() && IsCombiningMark(it.get()))
    it.Advance();

  // Next, maybe trim space at the current position.
  if (trim_whitespace) {
    // A mark combining with a space is renderable, so we'll prevent
    // trimming spaces with combining marks.
    while (!it.end() && IsSpace(it.get()) &&
           !IsCombiningMark(it.NextCodePoint())) {
      it.Advance();
    }
  }

  return it.array_pos();
}

HorizontalAlignment MaybeFlipForRTL(HorizontalAlignment alignment) {
  if (base::i18n::IsRTL() &&
      (alignment == gfx::ALIGN_LEFT || alignment == gfx::ALIGN_RIGHT)) {
    alignment =
        (alignment == gfx::ALIGN_LEFT) ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
  }
  return alignment;
}

Size GetStringSize(const std::u16string& text, const FontList& font_list) {
  return Size(GetStringWidth(text, font_list), font_list.GetHeight());
}

Insets AdjustVisualBorderForFont(const FontList& font_list,
                                 const Insets& desired_visual_padding) {
  Insets result = desired_visual_padding;
  const int baseline = font_list.GetBaseline();
  const int leading_space = baseline - font_list.GetCapHeight();
  const int descender = font_list.GetHeight() - baseline;
  result.set_top(std::max(0, result.top() - leading_space));
  result.set_bottom(std::max(0, result.bottom() - descender));
  return result;
}

int GetFontCapHeightCenterOffset(const gfx::FontList& original_font,
                                 const gfx::FontList& to_center) {
  const int original_cap_height = original_font.GetCapHeight();
  const int original_cap_leading =
      original_font.GetBaseline() - original_cap_height;
  const int to_center_cap_height = to_center.GetCapHeight();
  const int to_center_leading = to_center.GetBaseline() - to_center_cap_height;

  const int cap_height_diff = original_cap_height - to_center_cap_height;
  const int new_cap_top =
      original_cap_leading + base::ClampRound(cap_height_diff / 2.0f);
  const int new_top = new_cap_top - to_center_leading;

  // Since we assume the old font starts at zero, the new top is the adjustment.
  return new_top;
}

}  // namespace gfx
