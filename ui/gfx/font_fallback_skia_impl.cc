// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_skia_impl.h"

#include <stdint.h>

#include <set>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "skia/ext/font_utils.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace gfx {

namespace {

// Returns the unicode decomposition of |codepoint|, or an empty string when the
// codepoint has no decomposition.
std::u16string UnicodeDecomposeCodepoint(char32_t codepoint) {
  // Only Unicode scalar values have a decomposition. Unpaired surrogates, which
  // U16_NEXT() hands to this function for malformed input, are excluded here
  // because they cannot survive the UTF-8 round trip that the normalizer may
  // perform internally.
  if (!base::IsValidCodepoint(static_cast<int32_t>(codepoint))) {
    return std::u16string();
  }

  std::u16string codepoint_text;
  base::WriteUnicodeCharacter(static_cast<int32_t>(codepoint), &codepoint_text);
  std::u16string decomposed_text =
      base::i18n::IcuBridge::GetInstance().normalizer().Normalize(
          base::i18n::IcuBridge::Normalizer::NormalizationForm::NFD,
          codepoint_text);
  // NFD leaves a codepoint without a decomposition mapping unchanged, which is
  // the case where icu::Normalizer2::getDecomposition() reported failure.
  if (decomposed_text == codepoint_text) {
    return std::u16string();
  }

  return decomposed_text;
}

// Extracts every codepoint and its decomposed codepoints from unicode
// decomposition. Inserts in |codepoints| the set of codepoints in |text|.
void RetrieveCodepointsAndDecomposedCodepoints(std::u16string_view text,
                                               std::set<char32_t>* codepoints) {
  base::span<const char16_t> text_span(text);
  size_t offset = 0;
  while (offset < text.length()) {
    char32_t codepoint;
    U16_NEXT(text_span, offset, text_span.size(), codepoint);

    if (codepoints->insert(codepoint).second) {
      // For each codepoint, add the decomposed codepoints.
      for (char16_t code_unit : UnicodeDecomposeCodepoint(codepoint)) {
        codepoints->insert(code_unit);
      }
    }
  }
}

// Returns the amount of codepoint in |text| without a glyph representation in
// |typeface|. A codepoint is present if there is a corresponding glyph in
// typeface, or if there are glyphs for each of its decomposed codepoints.
size_t ComputeMissingGlyphsForGivenTypeface(std::u16string_view text,
                                            sk_sp<SkTypeface> typeface) {
  // Validate that every character has a known glyph in the font.
  base::span<const char16_t> text_span(text);
  size_t missing_glyphs = 0;
  size_t i = 0;
  while (i < text.length()) {
    char32_t codepoint;
    U16_NEXT(text_span, i, text_span.size(), codepoint);

    // The glyph is present in the font.
    if (typeface->unicharToGlyph(static_cast<SkUnichar>(codepoint)) != 0) {
      continue;
    }

    // Do not count missing codepoints when they are ignorable as they will be
    // ignored by the shaping engine.
    if (u_hasBinaryProperty(static_cast<int32_t>(codepoint),
                            UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) {
      continue;
    }

    // No glyph is present in the font for the codepoint. Try the decomposed
    // codepoints instead.
    const std::u16string decomposed_text = UnicodeDecomposeCodepoint(codepoint);
    if (!decomposed_text.empty()) {
      // Check that every decomposed codepoint is in the font.
      bool every_codepoint_found = true;
      for (char16_t code_unit : decomposed_text) {
        if (typeface->unicharToGlyph(static_cast<SkUnichar>(code_unit)) == 0) {
          every_codepoint_found = false;
          break;
        }
      }

      // The decomposed codepoints can be mapped to glyphs by the font.
      if (every_codepoint_found)
        continue;
    }

    // The current glyphs can't be find.
    ++missing_glyphs;
  }

  return missing_glyphs;
}

}  // namespace

sk_sp<SkTypeface> GetSkiaFallbackTypeface(const Font& template_font,
                                          const std::string& locale,
                                          std::u16string_view text) {
  if (text.empty())
    return nullptr;

  sk_sp<SkFontMgr> font_mgr(skia::DefaultFontMgr());

  const char* bcp47_locales[] = {locale.c_str()};
  int num_locales = locale.empty() ? 0 : 1;
  const char** locales = locale.empty() ? nullptr : bcp47_locales;

  const int font_weight = (template_font.GetWeight() == Font::Weight::INVALID)
                              ? static_cast<int>(Font::Weight::NORMAL)
                              : static_cast<int>(template_font.GetWeight());
  const bool italic = (template_font.GetStyle() & Font::ITALIC) != 0;
  SkFontStyle skia_style(
      font_weight, SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);

  std::set<SkTypefaceID> tested_typeface;
  sk_sp<SkTypeface> fallback_typeface;
  size_t fewest_missing_glyphs = text.length() + 1;

  // Retrieve the set of codepoints (or unicode decomposed codepoints) from
  // the input text.
  std::set<char32_t> codepoints;
  RetrieveCodepointsAndDecomposedCodepoints(text, &codepoints);

  // Determine which fallback font is given the fewer missing glyphs.
  for (char32_t codepoint : codepoints) {
    sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
        template_font.GetFontName().c_str(), skia_style, locales, num_locales,
        static_cast<SkUnichar>(codepoint)));
    // If the typeface is not found or was already tested, skip it.
    if (!typeface || !tested_typeface.insert(typeface->uniqueID()).second)
      continue;

    // Validate that every codepoint has a known glyph in the font.
    size_t missing_glyphs =
        ComputeMissingGlyphsForGivenTypeface(text, typeface);
    if (missing_glyphs < fewest_missing_glyphs) {
      fewest_missing_glyphs = missing_glyphs;
      fallback_typeface = typeface;
    }

    // The font is a valid fallback font for the given text.
    if (missing_glyphs == 0)
      break;
  }

  return fallback_typeface;
}

}  // namespace gfx
