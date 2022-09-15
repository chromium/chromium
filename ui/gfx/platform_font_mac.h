// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_MAC_H_
#define UI_GFX_PLATFORM_FONT_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

class GFX_EXPORT PlatformFontMac : public PlatformFont {
 public:
  // An enum indicating a type of system-specified font.
  //   - kGeneral: +[NSFont systemFontOfSize:(weight:)]
  //   - kMenu: +[NSFont menuFontOfSize:]
  //   - kToolTip: +[NSFont toolTipsFontOfSize:]
  enum class SystemFontType { kGeneral, kMenu, kToolTip };

  // Constructs a PlatformFontMac for a system-specified font of
  // |system_font_type| type. For a non-system-specified font, use any other
  // constructor.
  explicit PlatformFontMac(SystemFontType system_font_type);

  // Constructs a PlatformFontMac for containing the NSFont* |native_font|. Do
  // not call this for a system-specified font; use the |SystemFontType|
  // constructor for that. |native_font| must not be null.
  explicit PlatformFontMac(NativeFont native_font);

  // Constructs a PlatformFontMac representing the font with name |font_name|
  // and the size |font_size|. Do not call this for a system-specified font; use
  // the |SystemFontType| constructor for that.
  PlatformFontMac(const std::string& font_name,
                  int font_size);

  // Constructs a PlatformFontMac representing the font specified by |typeface|
  // and the size |font_size_pixels|. Do not call this for a system-specified
  // font; use the |SystemFontType| constructor for that.
  PlatformFontMac(sk_sp<SkTypeface> typeface,
                  int font_size_pixels,
                  const absl::optional<FontRenderParams>& params);

  PlatformFontMac(const PlatformFontMac&) = delete;
  PlatformFontMac& operator=(const PlatformFontMac&) = delete;

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
  sk_sp<SkTypeface> GetNativeSkTypeface() const override;

  // A utility function to get the weight of an NSFont. Used by the unit test.
  static Font::Weight GetFontWeightFromNSFontForTesting(NSFont* font);

 private:
  struct FontSpec {
    std::string name;  // Corresponds to -[NSFont fontFamily].
    int size;
    int style;
    Font::Weight weight;
  };

  PlatformFontMac(NativeFont font,
                  absl::optional<SystemFontType> system_font_type);

  PlatformFontMac(NativeFont font,
                  absl::optional<SystemFontType> system_font_type,
                  FontSpec spec);

  ~PlatformFontMac() override;

  // Calculates and caches the font metrics and initializes |render_params_|.
  void CalculateMetricsAndInitRenderParams();

  // Returns an autoreleased NSFont created with the passed-in specifications.
  NSFont* NSFontWithSpec(FontSpec font_spec) const;

  // The NSFont instance for this object. If this object was constructed from an
  // NSFont instance, this holds that NSFont instance. Otherwise this NSFont
  // instance is constructed from the name, size, and style. If there is no
  // active font that matched those criteria a default font is used.
  base::scoped_nsobject<NSFont> native_font_;

  // If the font is a system font, and if so, what kind.
  const absl::optional<SystemFontType> system_font_type_;

  // The name/size/style/weight quartet that specify the font. Initialized in
  // the constructors.
  const FontSpec font_spec_;

  // Cached metrics, generated in CalculateMetrics().
  int height_;
  int ascent_;
  int cap_height_;

  // Cached average width, generated in GetExpectedTextWidth().
  float average_width_ = 0.0;

  // Details about how the font should be rendered.
  FontRenderParams render_params_;
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_MAC_H_
