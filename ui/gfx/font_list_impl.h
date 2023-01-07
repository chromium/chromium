// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_LIST_IMPL_H_
#define UI_GFX_FONT_LIST_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/font.h"

namespace gfx {

// FontListImpl is designed to provide the implementation of FontList and
// intended to be used only from FontList.  You must not use this class
// directly.
//
// FontListImpl represents a list of fonts either in the form of Font vector or
// in the form of a string representing font names, styles, and size.
//
// FontListImpl could be initialized either way without conversion to the other
// form. The conversion to the other form is done only when asked to get the
// other form.
//
// For the format of font description string, see font_list.h for details.
class FontListImpl : public base::RefCounted<FontListImpl> {
 public:
  // Creates a font list from a string representing font names, styles, and
  // size.
  explicit FontListImpl(const std::string& font_description_string);

  // Creates a font list from font names, styles, size and weight.
  FontListImpl(const std::vector<std::string>& font_names,
               int font_style,
               int font_size,
               Font::Weight font_weight);

  // Creates a font list from a Font vector.
  // All fonts in this vector should have the same style and size.
  explicit FontListImpl(const std::vector<Font>& fonts);

  // Creates a font list from a Font.
  explicit FontListImpl(const Font& font);

  // Returns a new FontListImpl with the same font names but resized and the
  // given style and weight. |size_delta| is the size in pixels to add to the
  // current font size. |font_style| specifies the new style, which is a
  // bitmask of the values: Font::ITALIC and Font::UNDERLINE.
  FontListImpl* Derive(int size_delta,
                       int font_style,
                       Font::Weight weight) const;

  // Returns the height of this font list, which is max(ascent) + max(descent)
  // for all the fonts in the font list.
  int GetHeight() const;

  // Returns the baseline of this font list, which is max(baseline) for all the
  // fonts in the font list.
  int GetBaseline() const;

  // Returns the cap height of this font list.
  // Currently returns the cap height of the primary font.
  int GetCapHeight() const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the |Font::FontStyle| style flags for this font list.
  int GetFontStyle() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns the font weight.
  Font::Weight GetFontWeight() const;

  // Returns the Font vector.
  const std::vector<Font>& GetFonts() const;

  // Returns the first font in the list.
  const Font& GetPrimaryFont() const;

 private:
  friend class base::RefCounted<FontListImpl>;

  ~FontListImpl();

  // Extracts common font height and baseline into |common_height_| and
  // |common_baseline_|.
  void CacheCommonFontHeightAndBaseline() const;

  // Extracts font style and size into |font_style_| and |font_size_|.
  void CacheFontStyleAndSize() const;

  // A vector of Font. If FontListImpl is constructed with font description
  // string, |fonts_| is not initialized during construction. Instead, it is
  // computed lazily when user asked to get the font vector.
  mutable std::vector<Font> fonts_;

  // A string representing font names, styles, and sizes.
  // Please refer to the comments before class declaration for details on string
  // format.
  // If FontListImpl is constructed with a vector of font,
  // |font_description_string_| is not initialized during construction. Instead,
  // it is computed lazily when user asked to get the font description string.
  //
  // TODO(derat): Remove laziness so that this can be removed.
  mutable std::string font_description_string_;

  // The cached common height and baseline of the fonts in the font list.
  mutable int common_height_;
  mutable int common_baseline_;

  // Cached font style and size.
  mutable int font_style_;
  mutable int font_size_;
  mutable Font::Weight font_weight_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_LIST_IMPL_H_
