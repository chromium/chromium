// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_skia.h"

#include <algorithm>
#include <string>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMetrics.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/text_utils.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/system_fonts_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace gfx {
namespace {

// The font family name which is used when a user's application font for
// GNOME/KDE is a non-scalable one. The name should be listed in the
// IsFallbackFontAllowed function in skia/ext/SkFontHost_fontconfig_direct.cpp.
#if BUILDFLAG(IS_ANDROID)
const char kFallbackFontFamilyName[] = "serif";
#else
const char kFallbackFontFamilyName[] = "sans";
#endif

constexpr SkGlyphID kUnsupportedGlyph = 0;

// The default font, used for the default constructor.
base::LazyInstance<scoped_refptr<PlatformFontSkia>>::Leaky g_default_font =
    LAZY_INSTANCE_INITIALIZER;

// Creates a SkTypeface for the passed-in Font::FontStyle and family. If a
// fallback typeface is used instead of the requested family, |family| will be
// updated to contain the fallback's family name.
sk_sp<SkTypeface> CreateSkTypeface(bool italic,
                                   gfx::Font::Weight weight,
                                   std::string* family,
                                   bool* out_success) {
  DCHECK(family);
  TRACE_EVENT0("fonts", "gfx::CreateSkTypeface");

  const int font_weight = (weight == Font::Weight::INVALID)
                              ? static_cast<int>(Font::Weight::NORMAL)
                              : static_cast<int>(weight);
  SkFontStyle sk_style(
      font_weight, SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
  sk_sp<SkTypeface> typeface;
  {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("fonts"),
                 "skia::MakeTypefaceFromName", "family", *family);
    typeface = skia::MakeTypefaceFromName(family->c_str(), sk_style);
  }
  if (!typeface) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("fonts"),
                 "skia::MakeTypefaceFromName", "family",
                 kFallbackFontFamilyName);
    // A non-scalable font such as .pcf is specified. Fall back to a default
    // scalable font.
    typeface = skia::MakeTypefaceFromName(kFallbackFontFamilyName, sk_style);
    if (!typeface) {
      *out_success = false;
      return nullptr;
    }
    *family = kFallbackFontFamilyName;
  }
  *out_success = true;
  return typeface;
}

}  // namespace

std::string* PlatformFontSkia::default_font_description_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// PlatformFontSkia, public:

PlatformFontSkia::PlatformFontSkia() {
  EnsuresDefaultFontIsInitialized();
  InitFromPlatformFont(g_default_font.Get().get());
}

PlatformFontSkia::PlatformFontSkia(const std::string& font_name,
                                   int font_size_pixels) {
  FontRenderParamsQuery query;
  query.families.push_back(font_name);
  query.pixel_size = font_size_pixels;
  query.weight = Font::Weight::NORMAL;
  InitFromDetails(nullptr, font_name, font_size_pixels, Font::NORMAL,
                  query.weight, gfx::GetFontRenderParams(query, nullptr));
}

PlatformFontSkia::PlatformFontSkia(
    sk_sp<SkTypeface> typeface,
    int font_size_pixels,
    const std::optional<FontRenderParams>& params) {
  DCHECK(typeface);

  SkString family_name;
  typeface->getFamilyName(&family_name);

  SkFontStyle font_style = typeface->fontStyle();
  Font::Weight font_weight = FontWeightFromInt(font_style.weight());

  int style = typeface->isItalic() ? Font::ITALIC : Font::NORMAL;

  FontRenderParams actual_render_params;
  if (!params) {
    FontRenderParamsQuery query;
    query.families.push_back(family_name.c_str());
    query.pixel_size = font_size_pixels;
    query.weight = font_weight;
    actual_render_params = gfx::GetFontRenderParams(query, nullptr);
  } else {
    actual_render_params = params.value();
  }

  InitFromDetails(std::move(typeface), family_name.c_str(), font_size_pixels,
                  style, font_weight, actual_render_params);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontSkia, PlatformFont implementation:

// static
void PlatformFontSkia::EnsuresDefaultFontIsInitialized() {
  if (g_default_font.Get())
    return;

  std::string family = kFallbackFontFamilyName;
  int size_pixels = PlatformFont::kDefaultBaseFontSize;
  int style = Font::NORMAL;
  Font::Weight weight = Font::Weight::NORMAL;
  FontRenderParams params;

#if BUILDFLAG(IS_WIN)
  // On windows, the system default font is retrieved by using the GDI API
  // SystemParametersInfo(...) (see struct NONCLIENTMETRICS). The font
  // properties need to be converted as close as possible to a skia font.
  // The style must be kept (see http://crbug/989476).
  gfx::Font system_font = win::GetDefaultSystemFont();
  family = system_font.GetFontName();
  size_pixels = system_font.GetFontSize();
  style = system_font.GetStyle();
  weight = system_font.GetWeight();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
  // On Linux, LinuxUi is used to query the native toolkit (e.g.
  // GTK) for the default UI font.
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    const auto& font_settings = linux_ui->GetDefaultFontDescription();
    family = font_settings.family;
    size_pixels = font_settings.size_pixels;
    style = font_settings.style;
    weight = static_cast<Font::Weight>(font_settings.weight);
    params = linux_ui->GetDefaultFontRenderParams();
  } else
#endif
      if (default_font_description_) {
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS, a FontList font description string is stored as a
    // translatable resource and passed in via SetDefaultFontDescription().
    FontRenderParamsQuery query;
    CHECK(FontList::ParseDescription(*default_font_description_,
                                     &query.families, &query.style,
                                     &query.pixel_size, &query.weight))
        << "Failed to parse font description " << *default_font_description_;
    params = gfx::GetFontRenderParams(query, &family);
    size_pixels = query.pixel_size;
    style = query.style;
    weight = query.weight;
#else
        NOTREACHED_IN_MIGRATION();
#endif
  } else {
    params = gfx::GetFontRenderParams(FontRenderParamsQuery(), nullptr);
  }

  bool success = false;
  sk_sp<SkTypeface> typeface =
      CreateSkTypeface(style & Font::ITALIC, weight, &family, &success);

  // It's possible that the Skia interface is not longer able to proxy queries
  // to the browser process which make all requests to fail. Calling
  // MakeDefault() will try to get the default typeface; in case of failure it
  // returns an instance of SkEmptyTypeface. MakeDefault() should never fail.
  // See https://crbug.com/1287371 for details.
  if (!success) {
    typeface = skia::DefaultTypeface();
  }

  // Ensure there is a typeface available. If none is available, there is
  // nothing we can do about it and Chrome won't be able to work.
  CHECK(typeface.get()) << "No typeface available";

  g_default_font.Get() = new PlatformFontSkia(
      std::move(typeface), family, size_pixels, style, weight, params);
}

// static
void PlatformFontSkia::ReloadDefaultFont() {
  // Reset the scoped_refptr.
  g_default_font.Get() = nullptr;
}

// static
void PlatformFontSkia::SetDefaultFontDescription(
    const std::string& font_description) {
  delete default_font_description_;
  default_font_description_ = new std::string(font_description);
}

Font PlatformFontSkia::DeriveFont(int size_delta,
                                  int style,
                                  Font::Weight weight) const {
#if BUILDFLAG(IS_WIN)
  const int new_size = win::AdjustFontSize(font_size_pixels_, size_delta);
#else
  const int new_size = font_size_pixels_ + size_delta;
#endif

  DCHECK_GT(new_size, 0);

  // If the style changed, we may need to load a new face.
  std::string new_family = font_family_;
  bool success = true;
  sk_sp<SkTypeface> typeface =
      (weight == weight_ && style == style_)
          ? typeface_
          : CreateSkTypeface(style, weight, &new_family, &success);
  if (!success) {
    LOG(ERROR) << "Could not find any font: " << new_family << ", "
               << kFallbackFontFamilyName << ". Falling back to the default";
    return Font(new PlatformFontSkia);
  }

  FontRenderParamsQuery query;
  query.families.push_back(new_family);
  query.pixel_size = new_size;
  query.style = style;

  return Font(new PlatformFontSkia(std::move(typeface), new_family, new_size,
                                   style, weight,
                                   gfx::GetFontRenderParams(query, NULL)));
}

int PlatformFontSkia::GetHeight() {
  ComputeMetricsIfNecessary();
  return height_pixels_;
}

Font::Weight PlatformFontSkia::GetWeight() const {
  return weight_;
}

int PlatformFontSkia::GetBaseline() {
  ComputeMetricsIfNecessary();
  return ascent_pixels_;
}

int PlatformFontSkia::GetCapHeight() {
  ComputeMetricsIfNecessary();
  return cap_height_pixels_;
}

int PlatformFontSkia::GetExpectedTextWidth(int length) {
  ComputeMetricsIfNecessary();
  return round(static_cast<float>(length) * average_width_pixels_);
}

int PlatformFontSkia::GetStyle() const {
  return style_;
}

const std::string& PlatformFontSkia::GetFontName() const {
  return font_family_;
}

std::string PlatformFontSkia::GetActualFontName() const {
  SkString family_name;
  typeface_->getFamilyName(&family_name);
  return family_name.c_str();
}

int PlatformFontSkia::GetFontSize() const {
  return font_size_pixels_;
}

const FontRenderParams& PlatformFontSkia::GetFontRenderParams() {
  TRACE_EVENT0("fonts", "PlatformFontSkia::GetFontRenderParams");
  float current_scale_factor = GetFontRenderParamsDeviceScaleFactor();
  if (current_scale_factor != device_scale_factor_) {
    FontRenderParamsQuery query;
    query.families.push_back(font_family_);
    query.pixel_size = font_size_pixels_;
    query.style = style_;
    query.weight = weight_;
    query.device_scale_factor = current_scale_factor;
    font_render_params_ = gfx::GetFontRenderParams(query, nullptr);
    device_scale_factor_ = current_scale_factor;
  }
  return font_render_params_;
}

sk_sp<SkTypeface> PlatformFontSkia::GetNativeSkTypeface() const {
  DCHECK(typeface_);
  return sk_sp<SkTypeface>(typeface_);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontSkia, private:

PlatformFontSkia::PlatformFontSkia(sk_sp<SkTypeface> typeface,
                                   const std::string& family,
                                   int size_pixels,
                                   int style,
                                   Font::Weight weight,
                                   const FontRenderParams& render_params) {
  InitFromDetails(std::move(typeface), family, size_pixels, style, weight,
                  render_params);
}

PlatformFontSkia::~PlatformFontSkia() {}

void PlatformFontSkia::InitFromDetails(sk_sp<SkTypeface> typeface,
                                       const std::string& font_family,
                                       int font_size_pixels,
                                       int style,
                                       Font::Weight weight,
                                       const FontRenderParams& render_params) {
  TRACE_EVENT0("fonts", "PlatformFontSkia::InitFromDetails");
  DCHECK_GT(font_size_pixels, 0);

  font_family_ = font_family;
  bool success = true;
  typeface_ = typeface ? std::move(typeface)
                       : CreateSkTypeface(style & Font::ITALIC, weight,
                                          &font_family_, &success);

  if (!success) {
    EnsuresDefaultFontIsInitialized();
    InitFromPlatformFont(g_default_font.Get().get());
    return;
  }

  font_size_pixels_ = font_size_pixels;
  style_ = style;
  weight_ = weight;
  device_scale_factor_ = GetFontRenderParamsDeviceScaleFactor();
  font_render_params_ = render_params;
}

void PlatformFontSkia::InitFromPlatformFont(const PlatformFontSkia* other) {
  TRACE_EVENT0("fonts", "PlatformFontSkia::InitFromPlatformFont");
  typeface_ = other->typeface_;
  font_family_ = other->font_family_;
  font_size_pixels_ = other->font_size_pixels_;
  style_ = other->style_;
  weight_ = other->weight_;
  device_scale_factor_ = other->device_scale_factor_;
  font_render_params_ = other->font_render_params_;

  if (!other->metrics_need_computation_) {
    metrics_need_computation_ = false;
    ascent_pixels_ = other->ascent_pixels_;
    height_pixels_ = other->height_pixels_;
    cap_height_pixels_ = other->cap_height_pixels_;
    average_width_pixels_ = other->average_width_pixels_;
  }
}

void PlatformFontSkia::ComputeMetricsIfNecessary() {
  if (metrics_need_computation_) {
    TRACE_EVENT0("fonts", "PlatformFontSkia::ComputeMetricsIfNecessary");

    metrics_need_computation_ = false;

    SkFont font(typeface_, font_size_pixels_);
    const FontRenderParams& params = GetFontRenderParams();
    if (!params.antialiasing) {
      font.setEdging(SkFont::Edging::kAlias);
    } else if (params.subpixel_rendering ==
               FontRenderParams::SUBPIXEL_RENDERING_NONE) {
      font.setEdging(SkFont::Edging::kAntiAlias);
    } else {
      font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
    }

    font.setEmbolden(weight_ >= Font::Weight::BOLD && !typeface_->isBold());
    font.setSkewX((Font::ITALIC & style_) && !typeface_->isItalic()
                      ? -SK_Scalar1 / 4
                      : 0);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    ascent_pixels_ = SkScalarCeilToInt(-metrics.fAscent);
    cap_height_pixels_ = SkScalarCeilToInt(metrics.fCapHeight);

    // There is a mismatch between the way the PlatformFontWin was computing the
    // font height in pixel. The font height may vary by one pixel due to
    // decimal rounding.
    //     Windows Skia implements : ceil(descent - ascent)
    //     Linux Skia implements   : ceil(-ascent) + ceil(descent)
    // TODO(etienneb): Make both implementation consistent and fix the broken
    // unittests.
#if BUILDFLAG(IS_WIN)
    height_pixels_ = SkScalarCeilToInt(metrics.fDescent - metrics.fAscent);
#else
    height_pixels_ = ascent_pixels_ + SkScalarCeilToInt(metrics.fDescent);
#endif

    if (metrics.fAvgCharWidth) {
      average_width_pixels_ = SkScalarToDouble(metrics.fAvgCharWidth);
    } else {
      // Some Skia fonts manager do not compute the average character size
      // (e.g. Direct Write). The following code computes the average character
      // width the same way Blink (e.g. SimpleFontData) does. Use the width of
      // the letter 'x' when available, otherwise use the max character width.
      SkGlyphID glyph = typeface_->unicharToGlyph('x');
      if (glyph != kUnsupportedGlyph) {
        SkScalar sk_width;
        font.getWidths(&glyph, 1, &sk_width);
        average_width_pixels_ = SkScalarToDouble(sk_width);
      }
      if (!average_width_pixels_) {
        if (metrics.fMaxCharWidth) {
          average_width_pixels_ = SkScalarToDouble(metrics.fMaxCharWidth);
        } else {
          // Older version of the DirectWrite API doesn't implement support for
          // max char width. Fall back on a multiple of the ascent. This is
          // entirely arbitrary but comes pretty close to the expected value in
          // most cases.
          average_width_pixels_ = ascent_pixels_ * 2;
        }
      }
    }
    DCHECK_NE(average_width_pixels_, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontSkia;
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  TRACE_EVENT0("fonts", "PlatformFont::CreateFromNameAndSize");
  return new PlatformFontSkia(font_name, font_size);
}

// static
PlatformFont* PlatformFont::CreateFromSkTypeface(
    sk_sp<SkTypeface> typeface,
    int font_size_pixels,
    const std::optional<FontRenderParams>& params) {
  TRACE_EVENT0("fonts", "PlatformFont::CreateFromSkTypeface");
  return new PlatformFontSkia(typeface, font_size_pixels, params);
}

}  // namespace gfx
