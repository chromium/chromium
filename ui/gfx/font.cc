// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/gfx/platform_font.h"

#ifndef NDEBUG
#include <ostream>
#endif

namespace gfx {

////////////////////////////////////////////////////////////////////////////////
// Font, public:

Font::Font() : platform_font_(PlatformFont::CreateDefault()) {
}

Font::Font(const Font& other) : platform_font_(other.platform_font_) {
}

Font& Font::operator=(const Font& other) {
  platform_font_ = other.platform_font_;
  return *this;
}

#if BUILDFLAG(IS_APPLE)
Font::Font(CTFontRef ct_font)
    : platform_font_(PlatformFont::CreateFromCTFont(ct_font)) {}
#endif

Font::Font(PlatformFont* platform_font) : platform_font_(platform_font) {
}

Font::Font(const std::string& font_name, int font_size)
    : platform_font_(PlatformFont::CreateFromNameAndSize(font_name,
                                                         font_size)) {
}

Font::~Font() {
}

Font Font::Derive(int size_delta, int style, Font::Weight weight) const {
  if (size_delta == 0 && style == GetStyle() && weight == GetWeight())
    return *this;

  return platform_font_->DeriveFont(size_delta, style, weight);
}

int Font::GetHeight() const {
  return platform_font_->GetHeight();
}

int Font::GetBaseline() const {
  return platform_font_->GetBaseline();
}

int Font::GetCapHeight() const {
  return platform_font_->GetCapHeight();
}

int Font::GetExpectedTextWidth(int length) const {
  return platform_font_->GetExpectedTextWidth(length);
}

int Font::GetStyle() const {
  return platform_font_->GetStyle();
}

const std::string& Font::GetFontName() const {
  return platform_font_->GetFontName();
}

std::string Font::GetActualFontName() const {
  return platform_font_->GetActualFontName();
}

int Font::GetFontSize() const {
  return platform_font_->GetFontSize();
}

Font::Weight Font::GetWeight() const {
  return platform_font_->GetWeight();
}

const FontRenderParams& Font::GetFontRenderParams() const {
  return platform_font_->GetFontRenderParams();
}

#if BUILDFLAG(IS_APPLE)
CTFontRef Font::GetCTFont() const {
  return platform_font_->GetCTFont();
}
#endif

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& stream, const Font::Weight weight) {
  return stream << static_cast<int>(weight);
}
#endif

Font::Weight FontWeightFromInt(int weight) {
  static const Font::Weight weights[] = {
      Font::Weight::INVALID,  Font::Weight::THIN,   Font::Weight::EXTRA_LIGHT,
      Font::Weight::LIGHT,    Font::Weight::NORMAL, Font::Weight::MEDIUM,
      Font::Weight::SEMIBOLD, Font::Weight::BOLD,   Font::Weight::EXTRA_BOLD,
      Font::Weight::BLACK};

  const Font::Weight* next_bigger_weight = std::lower_bound(
      std::begin(weights), std::end(weights), weight,
      [](const Font::Weight& a, const int& b) {
        return static_cast<std::underlying_type<Font::Weight>::type>(a) < b;
      });
  if (next_bigger_weight != std::end(weights))
    return *next_bigger_weight;
  return Font::Weight::INVALID;
}

}  // namespace gfx
