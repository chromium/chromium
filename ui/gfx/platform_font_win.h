// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_WIN_H_
#define UI_GFX_PLATFORM_FONT_WIN_H_

#include <windows.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/platform_font.h"

struct IDWriteFactory;
struct IDWriteFont;

namespace gfx {

// Deprecation of PlatformFontWin (See http://crbug.com/944227).
extern GFX_EXPORT const base::Feature kPlatformFontSkiaOnWindows;

class GFX_EXPORT PlatformFontWin : public PlatformFont {
 public:
  PlatformFontWin();
  PlatformFontWin(const std::string& font_name, int font_size);

  // Wraps the provided SkTypeface without triggering a font rematch.
  PlatformFontWin(sk_sp<SkTypeface> typeface,
                  int font_size_pixels,
                  const base::Optional<FontRenderParams>& params);

  // Dialog units to pixels conversion.
  // See http://support.microsoft.com/kb/145994 for details.
  int horizontal_dlus_to_pixels(int dlus) const {
    return dlus * font_ref_->GetDluBaseX() / 4;
  }
  int vertical_dlus_to_pixels(int dlus)  const {
    return dlus * font_ref_->height() / 8;
  }

  // Returns the font name for the system locale. Some fonts, particularly
  // East Asian fonts, have different names per locale. If the localized font
  // name could not be retrieved, returns GetFontName().
  std::string GetLocalizedFontName() const;

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
  sk_sp<SkTypeface> GetNativeSkTypefaceIfAvailable() const override;

  NativeFont GetNativeFont() const;

  // Called once during initialization if we should be retrieving font metrics
  // from skia and DirectWrite.
  static void SetDirectWriteFactory(IDWriteFactory* factory);

 private:
  FRIEND_TEST_ALL_PREFIXES(PlatformFontWinTest, Metrics_SkiaVersusGDI);
  FRIEND_TEST_ALL_PREFIXES(PlatformFontWinTest, DirectWriteFontSubstitution);

  explicit PlatformFontWin(NativeFont native_font);
  ~PlatformFontWin() override;

  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, Font indirectly refers to the HFONT by way of
  // an HFontRef. That is, every Font has an HFontRef, which has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and Font can provide value semantics.
  class GFX_EXPORT HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int font_size,
             int height,
             int baseline,
             int cap_height,
             int ave_char_width,
             Font::Weight weight,
             int style);

    // Accessors
    HFONT hfont() const { return hfont_; }
    int height() const { return height_; }
    int baseline() const { return baseline_; }
    int cap_height() const { return cap_height_; }
    int ave_char_width() const { return ave_char_width_; }
    Font::Weight weight() const { return weight_; }
    int style() const { return style_; }
    const std::string& font_name() const { return font_name_; }
    int font_size() const { return font_size_; }
    int requested_font_size() const { return requested_font_size_; }

    // Returns the average character width in dialog units.
    int GetDluBaseX();

    // Helper to return the average character width using the text extent
    // technique mentioned here. http://support.microsoft.com/kb/125681.
    static int GetAverageCharWidthInDialogUnits(HFONT gdi_font);

   private:
    friend class base::RefCounted<HFontRef>;
    FRIEND_TEST_ALL_PREFIXES(PlatformFontWinTest, Metrics_SkiaVersusGDI);
    FRIEND_TEST_ALL_PREFIXES(PlatformFontWinTest, DirectWriteFontSubstitution);

    ~HFontRef();

    const HFONT hfont_;
    const int font_size_;
    const int height_;
    const int baseline_;
    const int cap_height_;
    const int ave_char_width_;
    const Font::Weight weight_;
    const int style_;
    // Average character width in dialog units. This is queried lazily from the
    // system, with an initial value of -1 meaning it hasn't yet been queried.
    int dlu_base_x_;
    std::string font_name_;

    // If the requested font size is not possible for the font, |font_size_|
    // will be different than |requested_font_size_|. This is stored separately
    // so that code that increases the font size in a loop will not cause the
    // loop to get stuck on the same size.
    int requested_font_size_;

    DISALLOW_COPY_AND_ASSIGN(HFontRef);
  };

  // Initializes this object with a copy of the specified HFONT.
  void InitWithCopyOfHFONT(HFONT hfont);

  // Initializes this object with the specified font name and size.
  void InitWithFontNameAndSize(const std::string& font_name,
                               int font_size);

  // Returns the GDI metrics for the font passed in.
  static void GetTextMetricsForFont(HDC hdc,
                                    HFONT font,
                                    TEXTMETRIC* text_metrics);

  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFontRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  // Creates and returns a new HFontRef from the specified HFONT. Uses provided
  // |font_metrics| instead of calculating new one.
  static HFontRef* CreateHFontRefFromGDI(HFONT font,
                                         const TEXTMETRIC& font_metrics);

  // Creates and returns a new HFontRef from the specified HFONT using metrics
  // from skia. Currently this is only used if we use DirectWrite for font
  // metrics.
  // |gdi_font| : Handle to the GDI font created via CreateFontIndirect.
  // |font_metrics| : The GDI font metrics retrieved via the GetTextMetrics
  // API. This is currently used to calculate the correct height of the font
  // in case we get a font created with a positive height.
  // A positive height represents the cell height (ascent + descent).
  // A negative height represents the character Em height which is cell
  // height minus the internal leading value.
  static PlatformFontWin::HFontRef* CreateHFontRefFromSkia(
      HFONT gdi_font,
      const TEXTMETRIC& font_metrics);

  // Takes control of a native font (e.g. from CreateFontIndirect()) and wraps
  // it in a Font object to manage its lifespan. Note that |hfont| may not be
  // valid after the call; use the returned Font object instead.
  static Font HFontToFont(HFONT hfont);

  // Creates a new PlatformFontWin with the specified HFontRef. Used when
  // constructing a Font from a HFONT we don't want to copy.
  explicit PlatformFontWin(HFontRef* hfont_ref);

    // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;

  // An optional typeface when the font is constructed from a typeface.
  sk_sp<SkTypeface> typeface_;

  DISALLOW_COPY_AND_ASSIGN(PlatformFontWin);
};

// Returns the family name for the |IDWriteFont| interface passed in.
// The family name is returned in the |family_name| parameter.
// Returns S_OK on success.
HRESULT GetFamilyNameFromDirectWriteFont(IDWriteFont* dwrite_font,
                                         base::string16* family_name);

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_WIN_H_
