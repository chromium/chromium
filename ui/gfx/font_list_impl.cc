// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/gfx/font_list.h"

namespace gfx {
namespace {

// Returns a font description from |families|, |style|, and |size_pixels|.
std::string BuildDescription(const std::vector<std::string>& families,
                             int style,
                             int size_pixels,
                             Font::Weight weight) {
  std::string description = base::JoinString(families, ",");
  description += ",";

  if (style & Font::ITALIC)
    description += "Italic ";
  switch (weight) {
    case Font::Weight::THIN:
      description += "Thin ";
      break;
    case Font::Weight::EXTRA_LIGHT:
      description += "Ultra-Light ";
      break;
    case Font::Weight::LIGHT:
      description += "Light ";
      break;
    case Font::Weight::MEDIUM:
      description += "Medium ";
      break;
    case Font::Weight::SEMIBOLD:
      description += "Semi-Bold ";
      break;
    case Font::Weight::BOLD:
      description += "Bold ";
      break;
    case Font::Weight::EXTRA_BOLD:
      description += "Ultra-Bold ";
      break;
    case Font::Weight::BLACK:
      description += "Heavy ";
      break;
    case Font::Weight::NORMAL:
    case Font::Weight::INVALID:
      break;
  }

  description += base::NumberToString(size_pixels);
  description += "px";

  return description;
}

}  // namespace

FontListImpl::FontListImpl(const std::string& font_description_string)
    : font_description_string_(font_description_string),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1),
      font_weight_(Font::Weight::INVALID) {
  DCHECK(!font_description_string.empty());
  // DCHECK description string ends with "px" for size in pixel.
  DCHECK(base::EndsWith(font_description_string, "px",
                        base::CompareCase::SENSITIVE));
}

FontListImpl::FontListImpl(const std::vector<std::string>& font_names,
                           int font_style,
                           int font_size,
                           Font::Weight font_weight)
    : font_description_string_(
          BuildDescription(font_names, font_style, font_size, font_weight)),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(font_style),
      font_size_(font_size),
      font_weight_(font_weight) {
  DCHECK(!font_names.empty());
  DCHECK(!font_names[0].empty());
}

FontListImpl::FontListImpl(const std::vector<Font>& fonts)
    : fonts_(fonts),
      common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1),
      font_weight_(Font::Weight::INVALID) {
  DCHECK(!fonts.empty());
  font_style_ = fonts[0].GetStyle();
  font_size_ = fonts[0].GetFontSize();
  font_weight_ = fonts[0].GetWeight();
#if DCHECK_IS_ON()
  for (size_t i = 1; i < fonts.size(); ++i) {
    DCHECK_EQ(fonts[i].GetStyle(), font_style_);
    DCHECK_EQ(fonts[i].GetFontSize(), font_size_);
  }
#endif
}

FontListImpl::FontListImpl(const Font& font)
    : common_height_(-1),
      common_baseline_(-1),
      font_style_(-1),
      font_size_(-1),
      font_weight_(Font::Weight::INVALID) {
  fonts_.push_back(font);
}

FontListImpl* FontListImpl::Derive(int size_delta,
                                   int font_style,
                                   Font::Weight weight) const {
  // If there is a font vector, derive from that.
  if (!fonts_.empty()) {
    std::vector<Font> fonts = fonts_;
    for (size_t i = 0; i < fonts.size(); ++i)
      fonts[i] = fonts[i].Derive(size_delta, font_style, weight);
    return new FontListImpl(fonts);
  }

  // Otherwise, parse the font description string to derive from it.
  std::vector<std::string> font_names;
  int old_size;
  int old_style;
  Font::Weight old_weight;
  CHECK(FontList::ParseDescription(font_description_string_, &font_names,
                                   &old_style, &old_size, &old_weight));
  const int size = std::max(1, old_size + size_delta);
  return new FontListImpl(font_names, font_style, size, weight);
}

int FontListImpl::GetHeight() const {
  if (common_height_ == -1)
    CacheCommonFontHeightAndBaseline();
  return common_height_;
}

int FontListImpl::GetBaseline() const {
  if (common_baseline_ == -1)
    CacheCommonFontHeightAndBaseline();
  return common_baseline_;
}

int FontListImpl::GetCapHeight() const {
  // Assume the primary font is used to render Latin characters.
  return GetPrimaryFont().GetCapHeight();
}

int FontListImpl::GetExpectedTextWidth(int length) const {
  // Rely on the primary font metrics for the time being.
  return GetPrimaryFont().GetExpectedTextWidth(length);
}

int FontListImpl::GetFontStyle() const {
  if (font_style_ == -1)
    CacheFontStyleAndSize();
  return font_style_;
}

int FontListImpl::GetFontSize() const {
  if (font_size_ == -1)
    CacheFontStyleAndSize();
  return font_size_;
}

Font::Weight FontListImpl::GetFontWeight() const {
  if (font_weight_ == Font::Weight::INVALID)
    CacheFontStyleAndSize();
  return font_weight_;
}

const std::vector<Font>& FontListImpl::GetFonts() const {
  if (fonts_.empty()) {
    DCHECK(!font_description_string_.empty());

    std::vector<std::string> font_names;
    // It's possible that Font::UNDERLINE is specified and it's already
    // stored in |font_style_| but |font_description_string_| doesn't have the
    // underline info.  So we should respect |font_style_| as long as it's
    // valid.
    int style = 0;
    CHECK(FontList::ParseDescription(font_description_string_, &font_names,
                                     &style, &font_size_, &font_weight_));
    if (font_style_ == -1)
      font_style_ = style;
    for (size_t i = 0; i < font_names.size(); ++i) {
      DCHECK(!font_names[i].empty());

      Font font(font_names[i], font_size_);
      if (font_style_ == Font::NORMAL && font_weight_ == Font::Weight::NORMAL)
        fonts_.push_back(font);
      else
        fonts_.push_back(font.Derive(0, font_style_, font_weight_));
    }
  }
  return fonts_;
}

const Font& FontListImpl::GetPrimaryFont() const {
  return GetFonts()[0];
}

FontListImpl::~FontListImpl() {}

void FontListImpl::CacheCommonFontHeightAndBaseline() const {
  int ascent = 0;
  int descent = 0;
  const std::vector<Font>& fonts = GetFonts();
  for (auto i = fonts.begin(); i != fonts.end(); ++i) {
    ascent = std::max(ascent, i->GetBaseline());
    descent = std::max(descent, i->GetHeight() - i->GetBaseline());
  }
  common_height_ = ascent + descent;
  common_baseline_ = ascent;
}

void FontListImpl::CacheFontStyleAndSize() const {
  if (!fonts_.empty()) {
    font_style_ = fonts_[0].GetStyle();
    font_size_ = fonts_[0].GetFontSize();
    font_weight_ = fonts_[0].GetWeight();
  } else {
    std::vector<std::string> font_names;
    CHECK(FontList::ParseDescription(font_description_string_, &font_names,
                                     &font_style_, &font_size_, &font_weight_));
  }
}

}  // namespace gfx
