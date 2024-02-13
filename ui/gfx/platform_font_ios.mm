// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_ios.h"

#import <UIKit/UIKit.h>

#include <cmath>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/ios/NSString+CrStringDrawing.h"

namespace gfx {

#if BUILDFLAG(USE_BLINK)

namespace {

std::string GetFamilyNameFromTypeface(sk_sp<SkTypeface> typeface) {
  SkString family;
  typeface->getFamilyName(&family);
  return family.c_str();
}

}  // namespace

#endif

////////////////////////////////////////////////////////////////////////////////
// PlatformFontIOS, public:

PlatformFontIOS::PlatformFontIOS() {
  font_size_ = UIFont.systemFontSize;
  style_ = Font::NORMAL;
  weight_ = Font::Weight::NORMAL;
  UIFont* system_font = [UIFont systemFontOfSize:font_size_];
  font_name_ = base::SysNSStringToUTF8(system_font.fontName);
  CalculateMetrics();
}

PlatformFontIOS::PlatformFontIOS(CTFontRef ct_font) {
  UIFont* font = base::apple::CFToNSPtrCast(ct_font);
  std::string font_name = base::SysNSStringToUTF8(font.fontName);
  InitWithNameSizeAndStyle(font_name, font.pointSize, Font::NORMAL,
                           Font::Weight::NORMAL);
}

PlatformFontIOS::PlatformFontIOS(const std::string& font_name, int font_size) {
  InitWithNameSizeAndStyle(font_name, font_size, Font::NORMAL,
                           Font::Weight::NORMAL);
}

#if BUILDFLAG(USE_BLINK)
PlatformFontIOS::PlatformFontIOS(
    sk_sp<SkTypeface> typeface,
    int font_size_pixels,
    const std::optional<FontRenderParams>& params) {
  InitWithNameSizeAndStyle(GetFamilyNameFromTypeface(typeface),
                           font_size_pixels,
                           (typeface->isItalic() ? Font::ITALIC : Font::NORMAL),
                           FontWeightFromInt(typeface->fontStyle().weight()));
}
#endif

////////////////////////////////////////////////////////////////////////////////
// PlatformFontIOS, PlatformFont implementation:

Font PlatformFontIOS::DeriveFont(int size_delta,
                                 int style,
                                 Font::Weight weight) const {
  return Font(
      new PlatformFontIOS(font_name_, font_size_ + size_delta, style, weight));
}

int PlatformFontIOS::GetHeight() {
  return height_;
}

int PlatformFontIOS::GetBaseline() {
  return ascent_;
}

int PlatformFontIOS::GetCapHeight() {
  return cap_height_;
}

int PlatformFontIOS::GetExpectedTextWidth(int length) {
  return length * average_width_;
}

int PlatformFontIOS::GetStyle() const {
  return style_;
}

Font::Weight PlatformFontIOS::GetWeight() const {
  return weight_;
}

const std::string& PlatformFontIOS::GetFontName() const {
  return font_name_;
}

std::string PlatformFontIOS::GetActualFontName() const {
  UIFont* font = base::apple::CFToNSPtrCast(GetCTFont());
  return base::SysNSStringToUTF8(font.familyName);
}

int PlatformFontIOS::GetFontSize() const {
  return font_size_;
}

const FontRenderParams& PlatformFontIOS::GetFontRenderParams() {
  return render_params_;
}

CTFontRef PlatformFontIOS::GetCTFont() const {
  UIFont* font = [UIFont fontWithName:base::SysUTF8ToNSString(font_name_)
                                 size:font_size_];

  UIFontDescriptor* descriptor = [font fontDescriptor];

  uint32_t traits = 0;
  if (weight_ >= Font::Weight::BOLD) {
    traits |= UIFontDescriptorTraitBold;
  }
  if (style_ == Font::ITALIC) {
    traits |= UIFontDescriptorTraitItalic;
  }
  descriptor = [descriptor fontDescriptorWithSymbolicTraits:traits];

  // 0.0 size means that the original size of the font specified in the
  // descriptor should be kept.
  UIFont* font_with_traits = [UIFont fontWithDescriptor:descriptor size:0.0];

  // UIFont's fontWithDescriptor:size: method can return nil if it cannot find a
  // font that matches the given descriptor.
  if (font_with_traits) {
    return base::apple::NSToCFPtrCast(font_with_traits);
  } else {
    return base::apple::NSToCFPtrCast(font);
  }
}

sk_sp<SkTypeface> PlatformFontIOS::GetNativeSkTypeface() const {
  return SkMakeTypefaceFromCTFont(GetCTFont());
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontIOS, private:

PlatformFontIOS::PlatformFontIOS(const std::string& font_name,
                                 int font_size,
                                 int style,
                                 Font::Weight weight) {
  InitWithNameSizeAndStyle(font_name, font_size, style, weight);
}

void PlatformFontIOS::InitWithNameSizeAndStyle(const std::string& font_name,
                                               int font_size,
                                               int style,
                                               Font::Weight weight) {
  font_name_ = font_name;
  font_size_ = font_size;
  style_ = style;
  weight_ = weight;
  CalculateMetrics();
}

void PlatformFontIOS::CalculateMetrics() {
  UIFont* font = base::apple::CFToNSPtrCast(GetCTFont());
  height_ = ceil(font.lineHeight);
  ascent_ = ceil(font.ascender);
  cap_height_ = ceil(font.capHeight);
  average_width_ = [@"x" cr_sizeWithFont:font].width;

  FontRenderParamsQuery query;
  query.families.push_back(font_name_);
  query.pixel_size = font_size_;
  query.style = style_;
  query.weight = weight_;
  render_params_ = gfx::GetFontRenderParams(query, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontIOS;
}

// static
PlatformFont* PlatformFont::CreateFromCTFont(CTFontRef ct_font) {
  return new PlatformFontIOS(ct_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontIOS(font_name, font_size);
}

#if BUILDFLAG(USE_BLINK)

// static
PlatformFont* PlatformFont::CreateFromSkTypeface(
    sk_sp<SkTypeface> typeface,
    int font_size_pixels,
    const std::optional<FontRenderParams>& params) {
  return new PlatformFontIOS(typeface, font_size_pixels, params);
}

#endif

}  // namespace gfx
