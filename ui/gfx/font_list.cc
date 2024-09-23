// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include <ostream>
#include <unordered_set>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/font_list_impl.h"

namespace {

// Font description of the default font set.
base::LazyInstance<std::string>::Leaky g_default_font_description =
    LAZY_INSTANCE_INITIALIZER;

// The default instance of gfx::FontListImpl.
base::LazyInstance<scoped_refptr<gfx::FontListImpl>>::Leaky g_default_impl =
    LAZY_INSTANCE_INITIALIZER;
bool g_default_impl_initialized = false;

#if !BUILDFLAG(IS_MAC)
bool IsFontFamilyAvailable(const std::string& family, SkFontMgr* font_manager) {
  return !!sk_sp<SkTypeface>(
      font_manager->matchFamilyStyle(family.c_str(), SkFontStyle()));
}
#endif

}  // namespace

namespace gfx {

// static
bool FontList::ParseDescription(const std::string& description,
                                std::vector<std::string>* families_out,
                                int* style_out,
                                int* size_pixels_out,
                                Font::Weight* weight_out) {
  DCHECK(families_out);
  DCHECK(style_out);
  DCHECK(size_pixels_out);
  DCHECK(weight_out);

  *families_out = base::SplitString(
      description, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (families_out->empty())
    return false;
  for (auto& family : *families_out)
    base::TrimWhitespaceASCII(family, base::TRIM_ALL, &family);

  // The last item is "[STYLE1] [STYLE2] [...] SIZE".
  std::vector<std::string> styles = base::SplitString(
      families_out->back(), base::kWhitespaceASCII,
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  families_out->pop_back();
  if (styles.empty())
    return false;

  // The size takes the form "<INT>px".
  std::string size_string = styles.back();
  styles.pop_back();
  if (!base::EndsWith(size_string, "px", base::CompareCase::SENSITIVE))
    return false;
  size_string.resize(size_string.size() - 2);
  if (!base::StringToInt(size_string, size_pixels_out) ||
      *size_pixels_out <= 0)
    return false;

  // Font supports ITALIC and weights; underline is supported via RenderText.
  *style_out = Font::NORMAL;
  *weight_out = Font::Weight::NORMAL;
  for (const auto& style_string : styles) {
    if (style_string == "Italic")
      *style_out |= Font::ITALIC;
    else if (style_string == "Thin")
      *weight_out = Font::Weight::THIN;
    else if (style_string == "Ultra-Light")
      *weight_out = Font::Weight::EXTRA_LIGHT;
    else if (style_string == "Light")
      *weight_out = Font::Weight::LIGHT;
    else if (style_string == "Normal")
      *weight_out = Font::Weight::NORMAL;
    else if (style_string == "Medium")
      *weight_out = Font::Weight::MEDIUM;
    else if (style_string == "Semi-Bold")
      *weight_out = Font::Weight::SEMIBOLD;
    else if (style_string == "Bold")
      *weight_out = Font::Weight::BOLD;
    else if (style_string == "Ultra-Bold")
      *weight_out = Font::Weight::EXTRA_BOLD;
    else if (style_string == "Heavy")
      *weight_out = Font::Weight::BLACK;
    else
      return false;
  }

  return true;
}

FontList::FontList() : impl_(GetDefaultImpl()) {}

FontList::FontList(const FontList& other) : impl_(other.impl_) {}

FontList::FontList(const std::string& font_description_string)
    : impl_(new FontListImpl(font_description_string)) {}

FontList::FontList(const std::vector<std::string>& font_names,
                   int font_style,
                   int font_size,
                   Font::Weight font_weight)
    : impl_(new FontListImpl(font_names, font_style, font_size, font_weight)) {}

FontList::FontList(const std::vector<Font>& fonts)
    : impl_(new FontListImpl(fonts)) {}

FontList::FontList(const Font& font) : impl_(new FontListImpl(font)) {}

FontList::~FontList() {}

FontList& FontList::operator=(const FontList& other) {
  impl_ = other.impl_;
  return *this;
}

// static
void FontList::SetDefaultFontDescription(const std::string& font_description) {
  // The description string must end with "px" for size in pixel, or must be
  // the empty string, which specifies to use a single default font.
  DCHECK(font_description.empty() ||
         base::EndsWith(font_description, "px", base::CompareCase::SENSITIVE));

  g_default_font_description.Get() = font_description;
  g_default_impl_initialized = false;
}

FontList FontList::Derive(int size_delta,
                          int font_style,
                          Font::Weight weight) const {
  return FontList(impl_->Derive(size_delta, font_style, weight));
}

FontList FontList::DeriveWithSizeDelta(int size_delta) const {
  return Derive(size_delta, GetFontStyle(), GetFontWeight());
}

FontList FontList::DeriveWithStyle(int font_style) const {
  return Derive(0, font_style, GetFontWeight());
}

FontList FontList::DeriveWithWeight(Font::Weight weight) const {
  return Derive(0, GetFontStyle(), weight);
}

FontList FontList::DeriveWithHeightUpperBound(int height) const {
  FontList font_list(*this);
  for (int font_size = font_list.GetFontSize(); font_size > 1; --font_size) {
    const int internal_leading =
        font_list.GetBaseline() - font_list.GetCapHeight();
    // Some platforms don't support getting the cap height, and simply return
    // the entire font ascent from GetCapHeight().  Centering the ascent makes
    // the font look too low, so if GetCapHeight() returns the ascent, center
    // the entire font height instead.
    const int space =
        height - ((internal_leading != 0) ?
                  font_list.GetCapHeight() : font_list.GetHeight());
    const int y_offset = space / 2 - internal_leading;
    const int space_at_bottom = height - (y_offset + font_list.GetHeight());
    if ((y_offset >= 0) && (space_at_bottom >= 0))
      break;
    font_list = font_list.DeriveWithSizeDelta(-1);
  }
  return font_list;
}

int FontList::GetHeight() const {
  return impl_->GetHeight();
}

int FontList::GetBaseline() const {
  return impl_->GetBaseline();
}

int FontList::GetCapHeight() const {
  return impl_->GetCapHeight();
}

int FontList::GetExpectedTextWidth(int length) const {
  return impl_->GetExpectedTextWidth(length);
}

int FontList::GetFontStyle() const {
  return impl_->GetFontStyle();
}

int FontList::GetFontSize() const {
  return impl_->GetFontSize();
}

Font::Weight FontList::GetFontWeight() const {
  return impl_->GetFontWeight();
}

const std::vector<Font>& FontList::GetFonts() const {
  return impl_->GetFonts();
}

const Font& FontList::GetPrimaryFont() const {
  return impl_->GetPrimaryFont();
}

FontList::FontList(FontListImpl* impl) : impl_(impl) {}

// static
const scoped_refptr<FontListImpl>& FontList::GetDefaultImpl() {
  // SetDefaultFontDescription() must be called and the default font description
  // must be set earlier than any call of this function.
  DCHECK(g_default_font_description.IsCreated())
      << "SetDefaultFontDescription has not been called.";

  if (!g_default_impl_initialized) {
    g_default_impl.Get() =
        g_default_font_description.Get().empty() ?
            new FontListImpl(Font()) :
            new FontListImpl(g_default_font_description.Get());
    g_default_impl_initialized = true;
  }

  return g_default_impl.Get();
}

// static
std::string FontList::FirstAvailableOrFirst(const std::string& font_name_list) {
  std::vector<std::string> families = base::SplitString(
      font_name_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (families.empty())
    return std::string();
  if (families.size() == 1)
    return families[0];
  sk_sp<SkFontMgr> fm(skia::DefaultFontMgr());
#if BUILDFLAG(IS_MAC)
  // We'd like to avoid SkFontMgr::matchFamilyStyle(), which opens a font
  // download dialog for available-but-not-installed fonts.

  // `available_size` is usually 200+. We make a hash set of available family
  // names in order to avoid at worst `available_size * families.size()` string
  // comparisons.
  const int available_size = fm->countFamilies();
  std::unordered_set<std::string> availables;
  for (int i = 0; i < available_size; ++i) {
    SkString name;
    fm->getFamilyName(i, &name);
    availables.emplace(name.data(), name.size());
  }
  for (const auto& family : families) {
    if (availables.contains(family)) {
      return family;
    }
  }
#else
  for (const auto& family : families) {
    if (IsFontFamilyAvailable(family, fm.get()))
      return family;
  }
#endif
  return families[0];
}

}  // namespace gfx
