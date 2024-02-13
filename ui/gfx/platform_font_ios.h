// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_IOS_H_
#define UI_GFX_PLATFORM_FONT_IOS_H_

#include "build/blink_buildflags.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class PlatformFontIOS : public PlatformFont {
 public:
  PlatformFontIOS();
  explicit PlatformFontIOS(CTFontRef ct_font);
  PlatformFontIOS(const std::string& font_name,
                  int font_size);
#if BUILDFLAG(USE_BLINK)
  // Constructs a PlatformFontIOS representing the font specified by |typeface|
  // and the size |font_size_pixels|. Do not call this for a system-specified
  // font; use the |SystemFontType| constructor for that.
  PlatformFontIOS(sk_sp<SkTypeface> typeface,
                  int font_size_pixels,
                  const std::optional<FontRenderParams>& params);
#endif
  PlatformFontIOS(const PlatformFontIOS&) = delete;
  PlatformFontIOS& operator=(const PlatformFontIOS&) = delete;

  // Overridden from PlatformFont:
  Font DeriveFont(int size_delta,
                  int style,
                  Font::Weight weight) const override;
  int GetHeight() override;
  Font::Weight GetWeight() const override;
  int GetBaseline() override;
  int GetCapHeight() override;
  int GetExpectedTextWidth(int length) override;
  int GetStyle() const override;
  const std::string& GetFontName() const override;
  std::string GetActualFontName() const override;
  int GetFontSize() const override;
  const FontRenderParams& GetFontRenderParams() override;
  CTFontRef GetCTFont() const override;
  sk_sp<SkTypeface> GetNativeSkTypeface() const override;

 private:
  PlatformFontIOS(const std::string& font_name,
                  int font_size,
                  int style,
                  Font::Weight weight);
  ~PlatformFontIOS() override = default;

  // Initialize the object with the specified parameters.
  void InitWithNameSizeAndStyle(const std::string& font_name,
                                int font_size,
                                int style,
                                Font::Weight weight);

  // Calculate and cache the font metrics.
  void CalculateMetrics();

  std::string font_name_;
  int font_size_;
  int style_;
  Font::Weight weight_;

  // Cached metrics, generated at construction.
  int height_;
  int ascent_;
  int cap_height_;
  int average_width_;

  // Details about how the font should be rendered.
  FontRenderParams render_params_;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_IOS_H_
