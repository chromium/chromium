// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_H_
#define UI_GFX_PLATFORM_FONT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
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
#if defined(OS_MACOSX)
  static constexpr int kDefaultBaseFontSize = 13;
#else
  static constexpr int kDefaultBaseFontSize = 12;
#endif

  // Creates an appropriate PlatformFont implementation.
  static PlatformFont* CreateDefault();
#if defined(OS_MACOSX) || defined(OS_IOS)
  static PlatformFont* CreateFromNativeFont(NativeFont native_font);
#endif
  // Creates a PlatformFont implementation with the specified |font_name|
  // (encoded in UTF-8) and |font_size| in pixels.
  static PlatformFont* CreateFromNameAndSize(const std::string& font_name,
                                             int font_size);

  // Creates a PlatformFont instance from the provided SkTypeface, ideally by
  // just wrapping it without triggering a new font match. Implemented for
  // PlatformFontWin and PlatformFontSkia, where only the latter provides true
  // wrapping of the provided SkTypeface, while PlatformFontWin creates a
  // PlatformFont object by extracting the family name and falls back to
  // CreateFromNameAndSize(). The FontRenderParams can be provided or they
  // will be determined by using gfx::GetFontRenderParams(...) otherwise.
  static PlatformFont* CreateFromSkTypeface(
      sk_sp<SkTypeface> typeface,
      int font_size,
      const base::Optional<FontRenderParams>& params);

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

#if defined(OS_MACOSX) || defined(OS_IOS)
  // Returns the native font handle.
  virtual NativeFont GetNativeFont() const = 0;
#endif

  // Returns the underlying Skia typeface if this PlatformFont instance is
  // backed by PlatformFontSkia, returns nullptr otherwise. Used in
  // RenderTextHarfBuzz for having access to the exact Skia typeface returned by
  // font fallback, as we would otherwise lose the handle to the correct
  // platform font instance.
  virtual sk_sp<SkTypeface> GetNativeSkTypefaceIfAvailable() const = 0;

 protected:
  virtual ~PlatformFont() {}

 private:
  friend class base::RefCounted<PlatformFont>;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_H_
