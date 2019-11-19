// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_H_
#define UI_GFX_FONT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

struct FontRenderParams;
class PlatformFont;

// Font provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
//
// Figure of font metrics:
//   +--------+-------------------+------------------+
//   |        |                   | internal leading |
//   |        | ascent (baseline) +------------------+
//   | height |                   | cap height       |
//   |        |-------------------+------------------+
//   |        | descent (height - baseline)          |
//   +--------+--------------------------------------+
class GFX_EXPORT Font {
 public:
  // The following constants indicate the font style.
  enum FontStyle {
    NORMAL = 0,
    ITALIC = 1,
    UNDERLINE = 2,
  };

  // Standard font weights as used in Pango and Windows. The values must match
  // https://msdn.microsoft.com/en-us/library/system.windows.fontweights(v=vs.110).aspx
  enum class Weight {
    INVALID = -1,
    THIN = 100,
    EXTRA_LIGHT = 200,
    LIGHT = 300,
    NORMAL = 400,
    MEDIUM = 500,
    SEMIBOLD = 600,
    BOLD = 700,
    EXTRA_BOLD = 800,
    BLACK = 900,
  };

  // Creates a font with the default name and style.
  Font();

  // Creates a font that is a clone of another font object.
  Font(const Font& other);
  Font& operator=(const Font& other);

#if defined(OS_MACOSX) || defined(OS_IOS)
  // Creates a font from the specified native font.
  explicit Font(NativeFont native_font);
#endif

  // Constructs a Font object with the specified PlatformFont object. The Font
  // object takes ownership of the PlatformFont object.
  explicit Font(PlatformFont* platform_font);

  // Creates a font with the specified name in UTF-8 and size in pixels.
  Font(const std::string& font_name, int font_size);

  ~Font();

  // Returns a new Font derived from the existing font.
  // |size_delta| is the size in pixels to add to the current font. For example,
  // a value of 5 results in a font 5 pixels bigger than this font.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: ITALIC and UNDERLINE.
  Font Derive(int size_delta, int style, Font::Weight weight) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  int GetHeight() const;

  // Returns the font weight.
  Font::Weight GetWeight() const;

  // Returns the baseline, or ascent, of the font.
  int GetBaseline() const;

  // Returns the cap height of the font.
  int GetCapHeight() const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call gfx::GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the style of the font.
  int GetStyle() const;

  // Returns the specified font name in UTF-8, without font mapping.
  const std::string& GetFontName() const;

  // Returns the actually used font name in UTF-8 after font mapping.
  std::string GetActualFontName() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns an object describing how the font should be rendered.
  const FontRenderParams& GetFontRenderParams() const;

#if defined(OS_MACOSX) || defined(OS_IOS)
  // Returns the native font handle.
  // Lifetime lore:
  // Mac:     The object is owned by the system and should not be released.
  NativeFont GetNativeFont() const;
#endif

  // Raw access to the underlying platform font implementation.
  PlatformFont* platform_font() const { return platform_font_.get(); }

 private:
  // Wrapped platform font implementation.
  scoped_refptr<PlatformFont> platform_font_;
};

#ifndef NDEBUG
GFX_EXPORT std::ostream& operator<<(std::ostream& stream,
                                    const Font::Weight weight);
#endif

// Returns the Font::Weight that matches |weight| or the next bigger one.
GFX_EXPORT Font::Weight FontWeightFromInt(int weight);

}  // namespace gfx

#endif  // UI_GFX_FONT_H_
