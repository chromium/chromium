// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_H_
#define UI_GFX_PLATFORM_FONT_H_

#include <optional>
#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class GFX_EXPORT PlatformFont : public base::RefCounted<PlatformFont> {
 public:
// The size of the font returned by CreateDefault() on a "default" platform
// configuration. This allows UI that wants to target a particular size of font
// to obtain that size for the majority of users, while still compensating for a
// user preference for a larger or smaller font.
#if BUILDFLAG(IS_APPLE)
  static constexpr int kDefaultBaseFontSize = 13;
#else
  static constexpr int kDefaultBaseFontSize = 12;
#endif

  // Takes a desired font size and returns the size delta to request from
  // ui::ResourceBundle that will result in font size plus any font size changes
  // made to account for locale or user settings.
  static constexpr int GetFontSizeDelta(int desired_font_size);

  // Takes a desired font size and returns the size delta to request from
  // ui::ResourceBundle that will result in exactly that font size, canceling
  // out any font size changes made to account for locale or user settings.
  static int GetFontSizeDeltaIgnoringUserOrLocaleSettings(
      int desired_font_size);

  // Creates an appropriate PlatformFont implementation.
  static PlatformFont* CreateDefault();
#if BUILDFLAG(IS_APPLE)
  static PlatformFont* CreateFromCTFont(CTFontRef ct_font);
#endif
  // Creates a PlatformFont implementation with the specified |font_name|
  // (encoded in UTF-8) and |font_size| in pixels.
  static PlatformFont* CreateFromNameAndSize(const std::string& font_name,
                                             int font_size);

  // Creates a PlatformFont instance from the provided SkTypeface, ideally by
  // just wrapping it without triggering a new font match. Implemented for
  // PlatformFontSkia which provides true wrapping of the provided SkTypeface.
  // The FontRenderParams can be provided or they will be determined by using
  // gfx::GetFontRenderParams(...) otherwise.
  static PlatformFont* CreateFromSkTypeface(
      sk_sp<SkTypeface> typeface,
      int font_size,
      const std::optional<FontRenderParams>& params);

  // Returns a new Font derived from the existing font.
  // |size_delta| is the size in pixels to add to the current font.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: ITALIC and UNDERLINE.
  // The weight parameter specifies the desired weight of the font.
  virtual Font DeriveFont(int size_delta,
                          int style,
                          Font::Weight weight) const = 0;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  virtual int GetHeight() = 0;

  // Returns the font weight.
  virtual Font::Weight GetWeight() const = 0;

  // Returns the baseline, or ascent, of the font.
  virtual int GetBaseline() = 0;

  // Returns the cap height of the font.
  virtual int GetCapHeight() = 0;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  virtual int GetExpectedTextWidth(int length) = 0;

  // Returns the style of the font.
  virtual int GetStyle() const = 0;

  // Returns the specified font name in UTF-8.
  virtual const std::string& GetFontName() const = 0;

  // Returns the actually used font name in UTF-8.
  virtual std::string GetActualFontName() const = 0;

  // Returns the font size in pixels.
  virtual int GetFontSize() const = 0;

  // Returns an object describing how the font should be rendered.
  virtual const FontRenderParams& GetFontRenderParams() = 0;

#if BUILDFLAG(IS_APPLE)
  // Returns the underlying CTFontRef.
  virtual CTFontRef GetCTFont() const = 0;
#endif

  // Returns the underlying Skia typeface. Used in RenderTextHarfBuzz for having
  // access to the exact Skia typeface returned by font fallback, as we would
  // otherwise lose the handle to the correct platform font instance.
  virtual sk_sp<SkTypeface> GetNativeSkTypeface() const = 0;

 protected:
  virtual ~PlatformFont() = default;

 private:
  friend class base::RefCounted<PlatformFont>;
};

constexpr int PlatformFont::GetFontSizeDelta(int desired_font_size) {
  return desired_font_size - kDefaultBaseFontSize;
}

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_H_
