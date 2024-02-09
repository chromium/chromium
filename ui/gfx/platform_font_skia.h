// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_SKIA_H_
#define UI_GFX_PLATFORM_FONT_SKIA_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class GFX_EXPORT PlatformFontSkia : public PlatformFont {
 public:
  // TODO(derat): Get rid of the default constructor in favor of using
  // FontList (which also has the concept of a default font but may contain
  // multiple font families) everywhere. See http://crbug.com/398885#c16.
  PlatformFontSkia();
  PlatformFontSkia(const std::string& font_name, int font_size_pixels);

  // Wraps the provided SkTypeface without triggering a font rematch.
  PlatformFontSkia(sk_sp<SkTypeface> typeface,
                   int font_size_pixels,
                   const std::optional<FontRenderParams>& params);

  PlatformFontSkia(const PlatformFontSkia&) = delete;
  PlatformFontSkia& operator=(const PlatformFontSkia&) = delete;

  // Initializes the default PlatformFont.
  static void EnsuresDefaultFontIsInitialized();

  // Resets and reloads the cached system font used by the default constructor.
  // This function is useful when the system font has changed, for example, when
  // the locale has changed.
  static void ReloadDefaultFont();

  // Sets the default font. |font_description| is a FontList font description;
  // only the first family will be used.
  // TODO(sergeyu): Remove this function. Currently it is used only on ChromeOS
  // to set the default font to the one loaded from resources.
  //
  static void SetDefaultFontDescription(const std::string& font_description);

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
  sk_sp<SkTypeface> GetNativeSkTypeface() const override;

#if BUILDFLAG(IS_APPLE)
  CTFontRef GetCTFont() const override;
#endif

 private:
  // Create a new instance of this object with the specified properties. Called
  // from DeriveFont.
  PlatformFontSkia(sk_sp<SkTypeface> typeface,
                   const std::string& family,
                   int size_pixels,
                   int style,
                   Font::Weight weight,
                   const FontRenderParams& params);
  ~PlatformFontSkia() override;

  // Initializes this object based on the passed-in details. If |typeface| is
  // empty, a new typeface will be loaded.
  void InitFromDetails(sk_sp<SkTypeface> typeface,
                       const std::string& font_family,
                       int font_size_pixels,
                       int style,
                       Font::Weight weight,
                       const FontRenderParams& params);

  // Initializes this object as a copy of another PlatformFontSkia.
  void InitFromPlatformFont(const PlatformFontSkia* other);

  // Computes the metrics if they have not yet been computed.
  void ComputeMetricsIfNecessary();

  sk_sp<SkTypeface> typeface_;

  // Additional information about the face.
  // Skia actually expects a family name and not a font name.
  std::string font_family_;
  int font_size_pixels_;
  int style_;
  float device_scale_factor_;

  // Information describing how the font should be rendered.
  FontRenderParams font_render_params_;

  // Cached metrics, generated on demand.
  bool metrics_need_computation_ = true;
  int ascent_pixels_;
  int height_pixels_;
  int cap_height_pixels_;
  double average_width_pixels_;
  Font::Weight weight_;

  // A font description string of the format used by FontList.
  static std::string* default_font_description_;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_SKIA_H_
