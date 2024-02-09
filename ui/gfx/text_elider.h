// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for eliding and formatting UI text.

#ifndef UI_GFX_TEXT_ELIDER_H_
#define UI_GFX_TEXT_ELIDER_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/text_constants.h"

namespace base {
class FilePath;
}

namespace gfx {
class FontList;

GFX_EXPORT extern const char kEllipsis[];
GFX_EXPORT extern const char16_t kEllipsisUTF16[];
GFX_EXPORT extern const char16_t kForwardSlash;

// Helper class to split + elide text, while respecting UTF-16 surrogate pairs
// and combining character sequences.
class GFX_EXPORT StringSlicer {
 public:
  // Warning: Retains a reference to |text| and |ellipsis|. They must have a
  // longer lifetime than the StringSlicer.
  //
  // Note: if |elide_whitespace| is std::nullopt, the default whitespace
  // elision strategy for the type of elision being done will be chosen.
  // Defaults are to trim for beginning and end elision; no trimming for middle
  // elision.
  StringSlicer(const std::u16string& text,
               const std::u16string& ellipsis,
               bool elide_in_middle,
               bool elide_at_beginning,
               std::optional<bool> elide_whitespace = std::nullopt);

  StringSlicer(const StringSlicer&) = delete;
  StringSlicer& operator=(const StringSlicer&) = delete;

  // Cuts |text_| to be at most |length| UTF-16 code units long. If
  // |elide_in_middle_| is true, the middle of the string is removed to leave
  // equal-length pieces from the beginning and end of the string; otherwise,
  // the end of the string is removed and only the beginning remains. If
  // |insert_ellipsis| is true, then an ellipsis character will be inserted at
  // the cut point (note that the ellipsis will does not count towards the
  // |length| limit).
  // Note: Characters may still be omitted even if |length| is the full string
  // length, if surrogate pairs fall on the split boundary.
  std::u16string CutString(size_t length, bool insert_ellipsis) const;

 private:
  // The text to be sliced.
  const raw_ref<const std::u16string, DanglingUntriaged> text_;

  // Ellipsis string to use.
  const raw_ref<const std::u16string, DanglingUntriaged> ellipsis_;

  // If true, the middle of the string will be elided.
  const bool elide_in_middle_;

  // If true, the beginning of the string will be elided.
  const bool elide_at_beginning_;

  // How whitespace around an elision point is handled.
  const bool elide_whitespace_;
};

// Elides |text| to fit the |available_pixel_width| with the specified behavior.
GFX_EXPORT std::u16string ElideText(const std::u16string& text,
                                    const gfx::FontList& font_list,
                                    float available_pixel_width,
                                    ElideBehavior elide_behavior);

// Elide a filename to fit a given pixel width, with an emphasis on not hiding
// the extension unless we have to. If filename contains a path, the path will
// be removed if filename doesn't fit into available_pixel_width. The elided
// filename is forced to have LTR directionality, which means that in RTL UI
// the elided filename is wrapped with LRE (Left-To-Right Embedding) mark and
// PDF (Pop Directional Formatting) mark.
GFX_EXPORT std::u16string ElideFilename(const base::FilePath& filename,
                                        const gfx::FontList& font_list,
                                        float available_pixel_width);

// Functions to elide strings when the font information is unknown. As opposed
// to the above functions, ElideString() and ElideRectangleString() operate in
// terms of character units, not pixels.

// If the size of |input| is more than |max_len|, this function returns
// true and |input| is shortened into |output| by removing chars in the
// middle (they are replaced with up to 3 dots, as size permits).
// Ex: ElideString(u"Hello", 10, &str) puts Hello in str and
// returns false.  ElideString(u"Hello my name is Tom", 10, &str)
// puts "Hell...Tom" in str and returns true.
// TODO(tsepez): Doesn't handle UTF-16 surrogate pairs properly.
// TODO(tsepez): Doesn't handle bidi properly.
GFX_EXPORT bool ElideString(const std::u16string& input,
                            size_t max_len,
                            std::u16string* output);

// Reformat |input| into |output| so that it fits into a |max_rows| by
// |max_cols| rectangle of characters.  Input newlines are respected, but
// lines that are too long are broken into pieces.  If |strict| is true,
// we break first at naturally occurring whitespace boundaries, otherwise
// we assume some other mechanism will do this in approximately the same
// spot after the fact.  If the word itself is too long, we always break
// intra-word (respecting UTF-16 surrogate pairs) as necessary. Truncation
// (indicated by an added 3 dots) occurs if the result is still too long.
//  Returns true if the input had to be truncated (and not just reformatted).
GFX_EXPORT bool ElideRectangleString(const std::u16string& input,
                                     size_t max_rows,
                                     size_t max_cols,
                                     bool strict,
                                     std::u16string* output);

// Indicates whether the |available_pixel_width| by |available_pixel_height|
// rectangle passed to |ElideRectangleText()| had insufficient space to
// accommodate the given |text|, leading to elision or truncation.
enum ReformattingResultFlags {
  INSUFFICIENT_SPACE_HORIZONTAL = 1 << 0,
  INSUFFICIENT_SPACE_VERTICAL = 1 << 1,
  INSUFFICIENT_SPACE_FOR_FIRST_WORD = 1 << 2,
};

// Reformats |text| into output vector |lines| so that the resulting text fits
// into an |available_pixel_width| by |available_pixel_height| rectangle with
// the specified |font_list|. Input newlines are respected, but lines that are
// too long are broken into pieces. For words that are too wide to fit on a
// single line, the wrapping behavior can be specified with the |wrap_behavior|
// param. Returns a combination of |ReformattingResultFlags| that indicate
// whether the given rectangle had insufficient space to accommodate |text|,
// leading to elision or truncation (and not just reformatting).
GFX_EXPORT int ElideRectangleText(const std::u16string& text,
                                  const gfx::FontList& font_list,
                                  float available_pixel_width,
                                  int available_pixel_height,
                                  WordWrapBehavior wrap_behavior,
                                  std::vector<std::u16string>* lines);

// Truncates |string| to |length| characters. This breaks the string according
// to the specified |break_type|, which must be either WORD_BREAK or
// CHARACTER_BREAK, and adds the horizontal ellipsis character (unicode
// character 0x2026) to render "...". The supplied string is returned if the
// string has |length| characters or less.
GFX_EXPORT std::u16string TruncateString(const std::u16string& string,
                                         size_t length,
                                         BreakType break_type);

}  // namespace gfx

#endif  // UI_GFX_TEXT_ELIDER_H_
