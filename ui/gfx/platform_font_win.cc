// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_win.h"

#include <dwrite.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <wchar.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "base/win/win_client_metrics.h"
#include "third_party/skia/include/core/SkFontLCDConfig.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/win/scoped_set_map_mode.h"

namespace {

// Sets style properties on |font_info| based on |font_style|.
void SetLogFontStyle(int font_style, LOGFONT* font_info) {
  font_info->lfUnderline = (font_style & gfx::Font::UNDERLINE) != 0;
  font_info->lfItalic = (font_style & gfx::Font::ITALIC) != 0;
}

gfx::Font::Weight ToGfxFontWeight(int weight) {
  return static_cast<gfx::Font::Weight>(weight);
}

// Uses the GDI interop functionality exposed by DirectWrite to find a
// matching DirectWrite font for the LOGFONT passed in. If we fail to
// find a direct match then we try the DirectWrite font substitution
// route to find a match.
// The contents of the LOGFONT pointer |font_info| may be modified on
// return.
HRESULT FindDirectWriteFontForLOGFONT(IDWriteFactory* factory,
                                      LOGFONT* font_info,
                                      IDWriteFont** dwrite_font) {
  Microsoft::WRL::ComPtr<IDWriteGdiInterop> gdi_interop;
  HRESULT hr = factory->GetGdiInterop(gdi_interop.GetAddressOf());
  if (FAILED(hr)) {
    CHECK(false);
    return hr;
  }

  hr = gdi_interop->CreateFontFromLOGFONT(font_info, dwrite_font);
  if (SUCCEEDED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IDWriteFontCollection> font_collection;
  hr = factory->GetSystemFontCollection(font_collection.GetAddressOf());
  if (FAILED(hr)) {
    CHECK(false);
    return hr;
  }

  // We try to find a matching font by triggering DirectWrite to substitute the
  // font passed in with a matching font (FontSubstitutes registry key)
  // If this succeeds we return the matched font.
  base::win::ScopedGDIObject<HFONT> font(::CreateFontIndirect(font_info));
  base::win::ScopedGetDC screen_dc(NULL);
  base::win::ScopedSelectObject scoped_font(screen_dc, font.get());

  Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
  hr = gdi_interop->CreateFontFaceFromHdc(screen_dc, font_face.GetAddressOf());
  if (FAILED(hr))
    return hr;

  LOGFONT converted_font = {0};
  hr = gdi_interop->ConvertFontFaceToLOGFONT(font_face.Get(), &converted_font);
  if (SUCCEEDED(hr)) {
    hr = font_collection->GetFontFromFontFace(font_face.Get(), dwrite_font);
    if (SUCCEEDED(hr)) {
      wcscpy_s(font_info->lfFaceName, arraysize(font_info->lfFaceName),
               converted_font.lfFaceName);
    }
  }
  return hr;
}

// Returns a matching IDWriteFont for the |font_info| passed in. If we fail
// to find a matching font, then we return the IDWriteFont corresponding to
// the default font on the system.
// Returns S_OK on success.
// The contents of the LOGFONT pointer |font_info| may be modified on
// return.
HRESULT GetMatchingDirectWriteFont(LOGFONT* font_info,
                                   bool italic,
                                   IDWriteFactory* factory,
                                   IDWriteFont** dwrite_font) {
  // First try the GDI compat route to get a matching DirectWrite font.
  // If that succeeds then we are good. If that fails then try and find a
  // match from the DirectWrite font collection.
  HRESULT hr = FindDirectWriteFontForLOGFONT(factory, font_info, dwrite_font);
  if (SUCCEEDED(hr))
    return hr;

  // Get a matching font from the system font collection exposed by
  // DirectWrite.
  Microsoft::WRL::ComPtr<IDWriteFontCollection> font_collection;
  hr = factory->GetSystemFontCollection(font_collection.GetAddressOf());
  if (FAILED(hr)) {
    CHECK(false);
    return hr;
  }

  // Steps as below:-
  // This mirrors skia.
  // 1. Attempt to find a DirectWrite font family based on the face name in the
  //    font. That may not work at all times, as the face name could be random
  //    GDI has its own font system where in it creates a font matching the
  //    characteristics in the LOGFONT structure passed into
  //    CreateFontIndirect. DirectWrite does not do that. If this succeeds then
  //    return the matching IDWriteFont from the family.
  // 2. If step 1 fails then repeat with the default system font. This has the
  //    same limitations with the face name as mentioned above.
  // 3. If step 2 fails then return the first family from the collection and
  //    use that.
  Microsoft::WRL::ComPtr<IDWriteFontFamily> font_family;
  BOOL exists = FALSE;
  uint32_t index = 0;
  hr = font_collection->FindFamilyName(font_info->lfFaceName, &index, &exists);
  // If we fail to find a match then try fallback to the default font on the
  // system. This is what skia does as well.
  if (FAILED(hr) || (index == UINT_MAX) || !exists) {
    NONCLIENTMETRICS metrics = {0};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                               sizeof(metrics),
                               &metrics,
                               0)) {
      CHECK(false);
      return E_FAIL;
    }

    if (wcsncmp(font_info->lfFaceName, metrics.lfMessageFont.lfFaceName,
                arraysize(font_info->lfFaceName))) {
      // First try the GDI compat route to get a matching DirectWrite font. If
      // that succeeds we are good. If not find a matching font from the font
      // collection.
      wcscpy_s(font_info->lfFaceName, arraysize(font_info->lfFaceName),
                metrics.lfMessageFont.lfFaceName);
      hr = FindDirectWriteFontForLOGFONT(factory, font_info, dwrite_font);
      if (SUCCEEDED(hr))
        return hr;

      // Best effort to find a matching font from the system font collection.
      hr = font_collection->FindFamilyName(metrics.lfMessageFont.lfFaceName,
                                           &index,
                                           &exists);
    }
  }

  if (index != UINT_MAX && exists) {
    hr = font_collection->GetFontFamily(index, font_family.GetAddressOf());
  } else {
    // If we fail to find a matching font, then fallback to the first font in
    // the list. This is what skia does as well.
    hr = font_collection->GetFontFamily(0, font_family.GetAddressOf());
  }

  if (FAILED(hr)) {
    CHECK(false);
    return hr;
  }

  DWRITE_FONT_WEIGHT weight =
      static_cast<DWRITE_FONT_WEIGHT>(font_info->lfWeight);
  DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
  DWRITE_FONT_STYLE style =
      (italic) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

  // The IDWriteFontFamily::GetFirstMatchingFont call fails on certain machines
  // for fonts like MS UI Gothic, Segoe UI, etc. It is not clear why these
  // fonts could be accessible to GDI and not to DirectWrite.
  // The code below adds some debug fields to help track down these failures.
  // 1. We get the matching font list for the font attributes passed in.
  // 2. We get the font count in the family with a debug alias variable.
  // 3. If GetFirstMatchingFont fails then we CHECK as before.
  // Next step would be to remove the CHECKs in this function and fallback to
  // GDI.
  // http://crbug.com/434425
  // TODO(ananta)
  // Remove the GetMatchingFonts and related code here once we get to a stable
  // state in canary.
  Microsoft::WRL::ComPtr<IDWriteFontList> matching_font_list;
  hr = font_family->GetMatchingFonts(weight, stretch, style,
                                     matching_font_list.GetAddressOf());
  uint32_t matching_font_count = 0;
  if (SUCCEEDED(hr))
    matching_font_count = matching_font_list->GetFontCount();

  hr = font_family->GetFirstMatchingFont(weight, stretch, style, dwrite_font);
  if (FAILED(hr)) {
    base::debug::Alias(&matching_font_count);
    CHECK(false);
  }

  base::string16 font_name;
  gfx::GetFamilyNameFromDirectWriteFont(*dwrite_font, &font_name);
  wcscpy_s(font_info->lfFaceName, arraysize(font_info->lfFaceName),
           font_name.c_str());
  return hr;
}

}  // namespace

namespace gfx {

namespace internal {

class SystemFonts {
 public:
  SystemFonts() {
    NONCLIENTMETRICS_XP metrics;
    base::win::GetNonClientMetrics(&metrics);

    // NOTE(dfried): When rendering Chrome, we do all of our own font scaling
    // based on a number of factors, but what Windows reports to us has some
    // (but not all) of these factors baked in, and not in a way that is
    // display-consistent.
    //
    // For example, if your system DPI is 192 (200%) but you connect a monitor
    // with a standard DPI (100%) then even if Chrome starts on the second
    // monitor, we will be told the system font is 24pt instead of 12pt.
    // Conversely, if the system DPI is set to 96 (100%) but all of our monitors
    // are currently at 150%, Windows will still report 12pt fonts.
    //
    // The same is true with Text Zoom (a new accessibility feature). If zoom is
    // set to 150%, then Windows will report a font size of 18pt. But again, we
    // already take Text Zoom into account when rendering, so we want to account
    // for that.
    //
    // Our system fonts are in DIPs, so we must always take what Windows gives
    // us, figure out which adjustments it's making (and undo them), make our
    // own adjustments for localization (for example, we always render Hindi 25%
    // larger for readability), and only then can we store (and report) the
    // system fonts.

    // Factor in/out scale adjustment that fall outside what we can access here.
    // This includes l10n adjustments and those we have to ask UWP or other COM
    // interfaces for (since we don't have dependencies on that code from this
    // module, and don't want to implicitly invoke COM for testing purposes if
    // we don't have to).
    gfx::PlatformFontWin::FontAdjustment font_adjustment;
    if (PlatformFontWin::adjust_font_callback_) {
      PlatformFontWin::adjust_font_callback_(&font_adjustment);
    }

    // Factor out system DPI scale that Windows will include in reported font
    // sizes. Note that these are (sadly) system-wide and do not reflect
    // specific displays' DPI.
    double system_scale = GetSystemScale();
    font_adjustment.font_scale /= system_scale;

    // Grab each of the fonts from the NONCLIENTMETRICS block, adjust it
    // appropriately, and store it in the font table.
    AddFont(gfx::PlatformFontWin::SystemFont::kCaption, font_adjustment,
            &metrics.lfCaptionFont);
    AddFont(gfx::PlatformFontWin::SystemFont::kSmallCaption, font_adjustment,
            &metrics.lfSmCaptionFont);
    AddFont(gfx::PlatformFontWin::SystemFont::kMenu, font_adjustment,
            &metrics.lfMenuFont);
    AddFont(gfx::PlatformFontWin::SystemFont::kMessage, font_adjustment,
            &metrics.lfMessageFont);
    AddFont(gfx::PlatformFontWin::SystemFont::kStatus, font_adjustment,
            &metrics.lfStatusFont);

    is_initialized_ = true;
  }

  const gfx::Font& GetFont(gfx::PlatformFontWin::SystemFont system_font) const {
    auto it = system_fonts_.find(system_font);
    DCHECK(it != system_fonts_.end())
        << "System font #" << static_cast<int>(system_font) << " not found!";
    DCHECK(it->second.GetNativeFont())
        << "Font for system font #" << static_cast<int>(system_font)
        << " has invalid handle.";
    return it->second;
  }

  static SystemFonts* Instance() {
    static base::NoDestructor<SystemFonts> instance;
    return instance.get();
  }

  static bool IsInitialized() { return is_initialized_; }

 private:
  void AddFont(gfx::PlatformFontWin::SystemFont system_font,
               const gfx::PlatformFontWin::FontAdjustment& font_adjustment,
               LOGFONT* logfont) {
    // Make adjustments to the font as necessary.
    PlatformFontWin::AdjustLOGFONT(font_adjustment, logfont);

    // Cap at minimum font size.
    logfont->lfHeight = PlatformFontWin::AdjustFontSize(logfont->lfHeight, 0);

    // Create the Font object.
    HFONT font = CreateFontIndirect(logfont);
    DLOG_ASSERT(font);
    system_fonts_.emplace(system_font, gfx::PlatformFontWin::HFontToFont(font));
  }

  // Returns the system DPI scale (standard DPI being 1.0).
  // TODO(dfried): move dpi.[h|cc] somewhere in base/win so we can share this
  // logic. However, note that the similar function in dpi.h is used many places
  // it ought not to be.
  static double GetSystemScale() {
    constexpr double kDefaultDPI = 96.0;
    base::win::ScopedGetDC screen_dc(nullptr);
    return GetDeviceCaps(screen_dc, LOGPIXELSY) / kDefaultDPI;
  }

  // Use a flat map for faster lookups.
  base::flat_map<gfx::PlatformFontWin::SystemFont, gfx::Font> system_fonts_;

  static bool is_initialized_;

  DISALLOW_COPY_AND_ASSIGN(SystemFonts);
};

// static
bool SystemFonts::is_initialized_ = false;

}  // namespace internal

// static
PlatformFontWin::HFontRef* PlatformFontWin::base_font_ref_;

// static
gfx::PlatformFontWin::AdjustFontCallback
    PlatformFontWin::adjust_font_callback_ = nullptr;

// static
gfx::PlatformFontWin::GetMinimumFontSizeCallback
    PlatformFontWin::get_minimum_font_size_callback_ = nullptr;

IDWriteFactory* PlatformFontWin::direct_write_factory_ = nullptr;

// TODO(ananta)
// Remove the CHECKs in this function once this stabilizes on the field.
HRESULT GetFamilyNameFromDirectWriteFont(IDWriteFont* dwrite_font,
                                         base::string16* family_name) {
  Microsoft::WRL::ComPtr<IDWriteFontFamily> font_family;
  HRESULT hr = dwrite_font->GetFontFamily(font_family.GetAddressOf());
  if (FAILED(hr))
    CHECK(false);

  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
  hr = font_family->GetFamilyNames(family_names.GetAddressOf());
  if (FAILED(hr))
    CHECK(false);

  // TODO(ananta)
  // Add support for retrieving the family for the current locale.
  wchar_t family_name_for_locale[MAX_PATH] = {0};
  hr = family_names->GetString(0, family_name_for_locale,
                               arraysize(family_name_for_locale));
  if (FAILED(hr))
    CHECK(false);

  *family_name = family_name_for_locale;
  return hr;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, public

PlatformFontWin::PlatformFontWin() : font_ref_(GetBaseFontRef()) {
}

PlatformFontWin::PlatformFontWin(NativeFont native_font) {
  InitWithCopyOfHFONT(native_font);
}

PlatformFontWin::PlatformFontWin(const std::string& font_name,
                                 int font_size) {
  InitWithFontNameAndSize(font_name, font_size);
}

// static
void PlatformFontWin::SetGetMinimumFontSizeCallback(
    GetMinimumFontSizeCallback callback) {
  DCHECK(!internal::SystemFonts::IsInitialized());
  get_minimum_font_size_callback_ = callback;
}

// static
void PlatformFontWin::SetAdjustFontCallback(AdjustFontCallback callback) {
  DCHECK(!internal::SystemFonts::IsInitialized());
  adjust_font_callback_ = callback;
}

// static
void PlatformFontWin::SetDirectWriteFactory(IDWriteFactory* factory) {
  // We grab a reference on the DirectWrite factory. This reference is
  // leaked, which is ok because skia leaks it as well.
  factory->AddRef();
  direct_write_factory_ = factory;
}

// static
bool PlatformFontWin::IsDirectWriteEnabled() {
  return direct_write_factory_ != nullptr;
}

// static
const Font& PlatformFontWin::GetSystemFont(SystemFont system_font) {
  return internal::SystemFonts::Instance()->GetFont(system_font);
}

// static
Font PlatformFontWin::AdjustExistingFont(
    NativeFont existing_font,
    const FontAdjustment& font_adjustment) {
  LOGFONT logfont;
  auto result = GetObject(existing_font, sizeof(logfont), &logfont);
  DCHECK(result);

  // Make the necessary adjustments.
  AdjustLOGFONT(font_adjustment, &logfont);

  // Cap at minimum font size.
  logfont.lfHeight = AdjustFontSize(logfont.lfHeight, 0);

  // Create the Font object.
  HFONT hfont = CreateFontIndirect(&logfont);
  DCHECK(hfont);
  return HFontToFont(hfont);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin, PlatformFont implementation:

Font PlatformFontWin::DeriveFont(int size_delta,
                                 int style,
                                 Font::Weight weight) const {
  LOGFONT font_info;
  GetObject(GetNativeFont(), sizeof(LOGFONT), &font_info);
  const int requested_font_size = font_ref_->requested_font_size();
  font_info.lfHeight = AdjustFontSize(-requested_font_size, size_delta);
  font_info.lfWeight = static_cast<LONG>(weight);
  SetLogFontStyle(style, &font_info);

  HFONT hfont = CreateFontIndirect(&font_info);
  return Font(new PlatformFontWin(CreateHFontRef(hfont)));
}

int PlatformFontWin::GetHeight() {
  return font_ref_->height();
}

Font::Weight PlatformFontWin::GetWeight() const {
  return font_ref_->weight();
}

int PlatformFontWin::GetBaseline() {
  return font_ref_->baseline();
}

int PlatformFontWin::GetCapHeight() {
  return font_ref_->cap_height();
}

int PlatformFontWin::GetExpectedTextWidth(int length) {
  return length * std::min(font_ref_->GetDluBaseX(),
                           font_ref_->ave_char_width());
}

int PlatformFontWin::GetStyle() const {
  return font_ref_->style();
}

const std::string& PlatformFontWin::GetFontName() const {
  return font_ref_->font_name();
}

std::string PlatformFontWin::GetActualFontNameForTesting() const {
  // With the current implementation on Windows, HFontRef::font_name() returns
  // the font name taken from the HFONT handle, but it's not the name that comes
  // from the font's metadata.  See http://crbug.com/327287
  return font_ref_->font_name();
}

std::string PlatformFontWin::GetLocalizedFontName() const {
  base::win::ScopedCreateDC memory_dc(CreateCompatibleDC(NULL));
  if (!memory_dc.Get())
    return GetFontName();

  // When a font has a localized name for a language matching the system
  // locale, GetTextFace() returns the localized name.
  base::win::ScopedSelectObject font(memory_dc.Get(), font_ref_->hfont());
  wchar_t localized_font_name[LF_FACESIZE];
  int length = GetTextFace(memory_dc.Get(), arraysize(localized_font_name),
                           &localized_font_name[0]);
  if (length <= 0)
    return GetFontName();
  return base::SysWideToUTF8(localized_font_name);
}

int PlatformFontWin::GetFontSize() const {
  return font_ref_->font_size();
}

const FontRenderParams& PlatformFontWin::GetFontRenderParams() {
  static const base::NoDestructor<FontRenderParams> params(
      gfx::GetFontRenderParams(FontRenderParamsQuery(), nullptr));
  return *params;
}

NativeFont PlatformFontWin::GetNativeFont() const {
  return font_ref_->hfont();
}

////////////////////////////////////////////////////////////////////////////////
// Font, private:

void PlatformFontWin::InitWithCopyOfHFONT(HFONT hfont) {
  DCHECK(hfont);
  LOGFONT font_info;
  GetObject(hfont, sizeof(LOGFONT), &font_info);
  font_ref_ = CreateHFontRef(CreateFontIndirect(&font_info));
}

void PlatformFontWin::InitWithFontNameAndSize(const std::string& font_name,
                                              int font_size) {
  HFONT hf = ::CreateFont(-font_size, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE,
                          base::UTF8ToUTF16(font_name).c_str());
  font_ref_ = CreateHFontRef(hf);
}

// static
void PlatformFontWin::GetTextMetricsForFont(HDC hdc,
                                            HFONT font,
                                            TEXTMETRIC* text_metrics) {
  base::win::ScopedSelectObject scoped_font(hdc, font);
  GetTextMetrics(hdc, text_metrics);
}

// static
PlatformFontWin::HFontRef* PlatformFontWin::GetBaseFontRef() {
  if (base_font_ref_ == nullptr) {
    // We'll delegate to our SystemFonts instance to give us the default
    // message font.
    PlatformFontWin* message_font =
        static_cast<PlatformFontWin*>(internal::SystemFonts::Instance()
                                          ->GetFont(SystemFont::kMessage)
                                          .platform_font());
    base_font_ref_ = message_font->font_ref_.get();
  }
  return base_font_ref_;
}

PlatformFontWin::HFontRef* PlatformFontWin::CreateHFontRef(HFONT font) {
  TEXTMETRIC font_metrics;

  {
    base::win::ScopedGetDC screen_dc(NULL);
    ScopedSetMapMode mode(screen_dc, MM_TEXT);
    GetTextMetricsForFont(screen_dc, font, &font_metrics);
  }

  if (IsDirectWriteEnabled())
    return CreateHFontRefFromSkia(font, font_metrics);

  return CreateHFontRefFromGDI(font, font_metrics);
}

PlatformFontWin::HFontRef* PlatformFontWin::CreateHFontRefFromGDI(
    HFONT font,
    const TEXTMETRIC& font_metrics) {
  const int height = std::max<int>(1, font_metrics.tmHeight);
  const int baseline = std::max<int>(1, font_metrics.tmAscent);
  const int cap_height =
      std::max<int>(1, font_metrics.tmAscent - font_metrics.tmInternalLeading);
  const int ave_char_width = std::max<int>(1, font_metrics.tmAveCharWidth);
  const int font_size =
      std::max<int>(1, font_metrics.tmHeight - font_metrics.tmInternalLeading);
  int style = 0;
  if (font_metrics.tmItalic)
    style |= Font::ITALIC;
  if (font_metrics.tmUnderlined)
    style |= Font::UNDERLINE;

  return new HFontRef(font, font_size, height, baseline, cap_height,
                      ave_char_width, ToGfxFontWeight(font_metrics.tmWeight),
                      style);
}

// static
PlatformFontWin::HFontRef* PlatformFontWin::CreateHFontRefFromSkia(
    HFONT gdi_font,
    const TEXTMETRIC& font_metrics) {
  LOGFONT font_info = {0};
  GetObject(gdi_font, sizeof(LOGFONT), &font_info);

  // If the font height is passed in as 0, assume the height to be -1 to ensure
  // that we return the metrics for a 1 point font.
  // If the font height is positive it represents the rasterized font's cell
  // height. Calculate the actual height accordingly.
  if (font_info.lfHeight > 0) {
    font_info.lfHeight =
        font_metrics.tmInternalLeading - font_metrics.tmHeight;
  } else if (font_info.lfHeight == 0) {
    font_info.lfHeight = -1;
  }

  if (font_info.lfWeight == 0) {
    font_info.lfWeight = static_cast<LONG>(Font::Weight::NORMAL);
  }

  const bool italic = font_info.lfItalic != 0;

  // Skia does not return all values we need for font metrics. For e.g.
  // the cap height which indicates the height of capital letters is not
  // returned even though it is returned by DirectWrite.
  // TODO(ananta)
  // Fix SkScalerContext_win_dw.cpp to return all metrics we need from
  // DirectWrite and remove the code here which retrieves metrics from
  // DirectWrite to calculate the cap height.
  Microsoft::WRL::ComPtr<IDWriteFont> dwrite_font;
  HRESULT hr = GetMatchingDirectWriteFont(
      &font_info, italic, direct_write_factory_, dwrite_font.GetAddressOf());
  if (FAILED(hr)) {
    CHECK(false);
    return nullptr;
  }

  DWRITE_FONT_METRICS dwrite_font_metrics = {0};
  dwrite_font->GetMetrics(&dwrite_font_metrics);

  SkFontStyle skia_font_style(font_info.lfWeight, SkFontStyle::kNormal_Width,
                              font_info.lfItalic ? SkFontStyle::kItalic_Slant
                                                 : SkFontStyle::kUpright_Slant);
  sk_sp<SkTypeface> skia_face(
      SkTypeface::MakeFromName(
          base::SysWideToUTF8(font_info.lfFaceName).c_str(),
                              skia_font_style));

  FontRenderParams font_params =
      gfx::GetFontRenderParams(FontRenderParamsQuery(), nullptr);
  SkFontLCDConfig::SetSubpixelOrder(
      FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          font_params.subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          font_params.subpixel_rendering));

  SkPaint paint;
  paint.setAntiAlias(font_params.antialiasing);
  paint.setTypeface(std::move(skia_face));
  paint.setTextSize(-font_info.lfHeight);
  SkPaint::FontMetrics skia_metrics;
  paint.getFontMetrics(&skia_metrics);

  // The calculations below are similar to those in the CreateHFontRef
  // function. The height, baseline and cap height are rounded up to ensure
  // that they match up closely with GDI.
  const int height = std::ceil(skia_metrics.fDescent - skia_metrics.fAscent);
  const int baseline = std::max<int>(1, std::ceil(-skia_metrics.fAscent));
  const int cap_height = std::ceil(paint.getTextSize() *
      static_cast<double>(dwrite_font_metrics.capHeight) /
          dwrite_font_metrics.designUnitsPerEm);

  // The metrics retrieved from skia don't have the average character width. In
  // any case if we get the average character width from skia then use that or
  // the average character width in the TEXTMETRIC structure.
  // TODO(ananta): Investigate whether it is possible to retrieve this value
  // from DirectWrite.
  const int ave_char_width =
      skia_metrics.fAvgCharWidth == 0 ? font_metrics.tmAveCharWidth
              : skia_metrics.fAvgCharWidth;

  int style = 0;
  if (italic)
    style |= Font::ITALIC;
  if (font_info.lfUnderline)
    style |= Font::UNDERLINE;

  // DirectWrite may have substituted the GDI font name with a fallback
  // font. Ensure that it is updated here.
  DeleteObject(gdi_font);
  gdi_font = ::CreateFontIndirect(&font_info);
  return new HFontRef(gdi_font, -font_info.lfHeight, height, baseline,
                      cap_height, ave_char_width,
                      ToGfxFontWeight(font_info.lfWeight), style);
}

// static
int PlatformFontWin::AdjustFontSize(int lf_height, int size_delta) {
  // Extract out the sign of |lf_height| - we'll add it back later.
  const int lf_sign = lf_height < 0 ? -1 : 1;
  lf_height = std::abs(lf_height);

  // Apply the size adjustment.
  lf_height += size_delta;

  // Make sure |lf_height| is not smaller than allowed min allowed font size.
  int min_font_size = 0;
  if (get_minimum_font_size_callback_) {
    min_font_size = get_minimum_font_size_callback_();
    DCHECK_GE(min_font_size, 0);
  }
  lf_height = std::max(min_font_size, lf_height);

  // Add back the sign.
  return lf_sign * lf_height;
}

// static
void PlatformFontWin::AdjustLOGFONT(
    const gfx::PlatformFontWin::FontAdjustment& font_adjustment,
    LOGFONT* logfont) {
  DCHECK_GT(font_adjustment.font_scale, 0.0);
  LONG new_height =
      LONG{std::round(logfont->lfHeight * font_adjustment.font_scale)};
  if (logfont->lfHeight && !new_height)
    new_height = logfont->lfHeight > 0 ? 1 : -1;
  logfont->lfHeight = new_height;
  if (!font_adjustment.font_family_override.empty()) {
    auto result = wcscpy_s(logfont->lfFaceName,
                           font_adjustment.font_family_override.c_str());
    DCHECK_EQ(0, result) << "Font name " << font_adjustment.font_family_override
                         << " cannot be copied into LOGFONT structure.";
  }
}

// static
Font PlatformFontWin::HFontToFont(HFONT hfont) {
  return Font(new PlatformFontWin(CreateHFontRef(hfont)));
}

PlatformFontWin::PlatformFontWin(HFontRef* hfont_ref) : font_ref_(hfont_ref) {
}

PlatformFontWin::~PlatformFontWin() {
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontWin::HFontRef:

PlatformFontWin::HFontRef::HFontRef(HFONT hfont,
                                    int font_size,
                                    int height,
                                    int baseline,
                                    int cap_height,
                                    int ave_char_width,
                                    Font::Weight weight,
                                    int style)
    : hfont_(hfont),
      font_size_(font_size),
      height_(height),
      baseline_(baseline),
      cap_height_(cap_height),
      ave_char_width_(ave_char_width),
      weight_(weight),
      style_(style),
      dlu_base_x_(-1),
      requested_font_size_(font_size) {
  DLOG_ASSERT(hfont);

  LOGFONT font_info;
  GetObject(hfont_, sizeof(LOGFONT), &font_info);
  font_name_ = base::UTF16ToUTF8(base::string16(font_info.lfFaceName));

  // Retrieve the font size from the GetTextMetrics API instead of referencing
  // it from the LOGFONT structure. This is because the height as reported by
  // the LOGFONT structure is not always correct. For small fonts with size 1
  // the LOGFONT structure reports the height as -1, while the actual font size
  // is different. (2 on my XP machine).
  base::win::ScopedGetDC screen_dc(NULL);
  TEXTMETRIC font_metrics = {0};
  PlatformFontWin::GetTextMetricsForFont(screen_dc, hfont_, &font_metrics);
  requested_font_size_ = font_metrics.tmHeight - font_metrics.tmInternalLeading;
}

int PlatformFontWin::HFontRef::GetDluBaseX() {
  if (dlu_base_x_ != -1)
    return dlu_base_x_;

  dlu_base_x_ = GetAverageCharWidthInDialogUnits(hfont_);
  return dlu_base_x_;
}

// static
int PlatformFontWin::HFontRef::GetAverageCharWidthInDialogUnits(
    HFONT gdi_font) {
  base::win::ScopedGetDC screen_dc(NULL);
  base::win::ScopedSelectObject font(screen_dc, gdi_font);
  ScopedSetMapMode mode(screen_dc, MM_TEXT);

  // Yes, this is how Microsoft recommends calculating the dialog unit
  // conversions. See: http://support.microsoft.com/kb/125681
  SIZE ave_text_size;
  GetTextExtentPoint32(screen_dc,
                       L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                       52, &ave_text_size);
  int dlu_base_x = (ave_text_size.cx / 26 + 1) / 2;

  DCHECK_NE(dlu_base_x, -1);
  return dlu_base_x;
}

PlatformFontWin::HFontRef::~HFontRef() {
  DeleteObject(hfont_);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontWin;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontWin(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontWin(font_name, font_size);
}

}  // namespace gfx
