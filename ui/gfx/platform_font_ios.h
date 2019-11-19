// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_IOS_H_
#define UI_GFX_PLATFORM_FONT_IOS_H_

#include "base/macros.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class PlatformFontIOS : public PlatformFont {
 public:
  PlatformFontIOS();
  explicit PlatformFontIOS(NativeFont native_font);
  PlatformFontIOS(const std::string& font_name,
                  int font_size);

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
  NativeFont GetNativeFont() const override;
  sk_sp<SkTypeface> GetNativeSkTypefaceIfAvailable() const override;

 private:
  PlatformFontIOS(const std::string& font_name,
                  int font_size,
                  int style,
                  Font::Weight weight);
  ~PlatformFontIOS() override {}

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

  DISALLOW_COPY_AND_ASSIGN(PlatformFontIOS);
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_IOS_H_
