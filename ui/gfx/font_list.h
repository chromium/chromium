// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_LIST_H_
#define UI_GFX_FONT_LIST_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class FontListImpl;

// FontList represents a list of fonts and provides metrics which are common
// across the fonts.  FontList is copyable and quite cheap to copy.
//
// The format of font description strings is a subset of that used by Pango, as
// described at
// http://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
//
// Pango font description strings should not be passed directly into FontLists.
//
// The format is "<FONT_FAMILY_LIST>,[STYLES] <SIZE>", where:
// - FONT_FAMILY_LIST is a comma-separated list of font family names,
// - STYLES is an optional space-separated list of style names (case-sensitive
//   "Italic" "Ultra-Light" "Light" "Normal" "Semi-Bold" "Bold" "Ultra-Bold"
//   "Heavy" are supported), and
// - SIZE is an integer font size in pixels with the suffix "px"
//
// Here are examples of valid font description strings:
// - "Arial, Helvetica, Italic Semi-Bold 14px"
// - "Arial, 14px"
class GFX_EXPORT FontList {
 public:
  // Parses a FontList description string into |families_out|, |style_out| (a
  // bitfield of gfx::Font::Style values), |size_pixels_out| and |weight_out|.
  // Returns true if the string is properly-formed.
  static bool ParseDescription(const std::string& description,
                               std::vector<std::string>* families_out,
                               int* style_out,
                               int* size_pixels_out,
                               Font::Weight* weight_out);

  // Creates a font list with default font names, size and style, which are
  // specified by SetDefaultFontDescription().
  FontList();

  // Creates a font list that is a clone of another font list.
  FontList(const FontList& other);

  // Creates a font list from a string representing font names, styles, and
  // size.
  explicit FontList(const std::string& font_description_string);

  // Creates a font list from font names, styles, size and weight.
  FontList(const std::vector<std::string>& font_names,
           int font_style,
           int font_size,
           Font::Weight font_weight);

  // Creates a font list from a Font vector.
  // All fonts in this vector should have the same style and size.
  explicit FontList(const std::vector<Font>& fonts);

  // Creates a font list from a Font.
  explicit FontList(const Font& font);

  ~FontList();

  // Copies the given font list into this object.
  FontList& operator=(const FontList& other);

  // Sets the description string for default FontList construction. If it's
  // empty, FontList will initialize using the default Font constructor.
  //
  // The client code must call this function before any call of the default
  // constructor. This should be done on the UI thread.
  //
  // ui::ResourceBundle may call this function more than once when UI language
  // is changed.
  //
  // Unit Tests should use ScopedDefaultFontDescription instead of calling this
  // directly, to avoid leaving the default font description in an unexpected
  // state for tests that run in the same process.
  static void SetDefaultFontDescription(const std::string& font_description);

  // Returns a new FontList with the same font names but resized and the given
  // style and weight. |size_delta| is the size in pixels to add to the current
  // font size. |font_style| specifies the new style, which is a bitmask of the
  // values: Font::ITALIC and Font::UNDERLINE. |weight| is the requested font
  // weight.
  FontList Derive(int size_delta,
                  int font_style,
                  Font::Weight weight) const;

  // Returns a new FontList with the same font names and style but resized.
  // |size_delta| is the size in pixels to add to the current font size.
  FontList DeriveWithSizeDelta(int size_delta) const;

  // Returns a new FontList with the same font names, weight and size but the
  // given style. |font_style| specifies the new style, which is a bitmask of
  // the values: Font::ITALIC and Font::UNDERLINE.
  FontList DeriveWithStyle(int font_style) const;

  // Returns a new FontList with the same font name, size and style but with
  // the given weight.
  FontList DeriveWithWeight(Font::Weight weight) const;

  // Shrinks the font size until the font list fits within |height| while
  // having its cap height vertically centered. Returns a new FontList with
  // the correct height.
  //
  // The expected layout:
  //   +--------+-----------------------------------------------+------------+
  //   |        | y offset                                      | space      |
  //   |        +--------+-------------------+------------------+ above      |
  //   |        |        |                   | internal leading | cap height |
  //   | box    | font   | ascent (baseline) +------------------+------------+
  //   | height | height |                   | cap height                    |
  //   |        |        |-------------------+------------------+------------+
  //   |        |        | descent (height - baseline)          | space      |
  //   |        +--------+--------------------------------------+ below      |
  //   |        | space at bottom                               | cap height |
  //   +--------+-----------------------------------------------+------------+
  // Goal:
  //     center of box height == center of cap height
  //     (i.e. space above cap height == space below cap height)
  // Restrictions:
  //     y offset >= 0
  //     space at bottom >= 0
  //     (i.e. Entire font must be visible inside the box.)
  FontList DeriveWithHeightUpperBound(int height) const;

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

  // Returns the first available font name. If there is no available font,
  // returns the first font name. Empty entries are ignored.
  // Used by Blink and webui to pick the primary standard/serif/sans/fixed/etc.
  // fonts from region-specific IDS lists.
  static std::string FirstAvailableOrFirst(const std::string& font_name_list);

 private:
  explicit FontList(FontListImpl* impl);

  static const scoped_refptr<FontListImpl>& GetDefaultImpl();

  scoped_refptr<FontListImpl> impl_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_LIST_H_
