// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_skia_impl.h"

#include <set>
#include <string>

#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace gfx {

namespace {

// Returns true when the codepoint has an unicode decomposition and store
// the decomposed string into |output|.
bool UnicodeDecomposeCodepoint(UChar32 codepoint, icu::UnicodeString* output) {
  static const icu::Normalizer2* normalizer = nullptr;

  UErrorCode error = U_ZERO_ERROR;
  if (!normalizer) {
    normalizer = icu::Normalizer2::getNFDInstance(error);
    if (U_FAILURE(error))
      return false;
    DCHECK(normalizer);
  }

  return normalizer->getDecomposition(codepoint, *output);
}

// Extracts every codepoint and its decomposed codepoints from unicode
// decomposition. Inserts in |codepoints| the set of codepoints in |text|.
void RetrieveCodepointsAndDecomposedCodepoints(base::StringPiece16 text,
                                               std::set<UChar32>* codepoints) {
  size_t offset = 0;
  while (offset < text.length()) {
    UChar32 codepoint;
    U16_NEXT(text.data(), offset, text.length(), codepoint);

    if (codepoints->insert(codepoint).second) {
      // For each codepoint, add the decomposed codepoints.
      icu::UnicodeString decomposed_text;
      if (UnicodeDecomposeCodepoint(codepoint, &decomposed_text)) {
        for (int i = 0; i < decomposed_text.length(); ++i) {
          codepoints->insert(decomposed_text[i]);
        }
      }
    }
  }
}

// Returns the amount of codepoint in |text| without a glyph representation in
// |typeface|. A codepoint is present if there is a corresponding glyph in
// typeface, or if there are glyphs for each of its decomposed codepoints.
size_t ComputeMissingGlyphsForGivenTypeface(base::StringPiece16 text,
                                            sk_sp<SkTypeface> typeface) {
  // Validate that every character has a known glyph in the font.
  size_t missing_glyphs = 0;
  size_t i = 0;
  while (i < text.length()) {
    UChar32 codepoint;
    U16_NEXT(text.data(), i, text.length(), codepoint);

    // The glyph is present in the font.
    if (typeface->unicharToGlyph(codepoint) != 0)
      continue;

    // No glyph is present in the font for the codepoint. Try the decomposed
    // codepoints instead.
    icu::UnicodeString decomposed_text;
    if (UnicodeDecomposeCodepoint(codepoint, &decomposed_text) &&
        !decomposed_text.isEmpty()) {
      // Check that every decomposed codepoint is in the font.
      bool every_codepoint_found = true;
      for (int offset = 0; offset < decomposed_text.length(); ++offset) {
        if (typeface->unicharToGlyph(decomposed_text[offset]) == 0) {
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
                                          base::StringPiece16 text) {
  if (text.empty())
    return nullptr;

  sk_sp<SkFontMgr> font_mgr(SkFontMgr::RefDefault());

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

  std::set<SkFontID> tested_typeface;
  sk_sp<SkTypeface> fallback_typeface;
  size_t fewest_missing_glyphs = text.length() + 1;

  // Retrieve the set of codepoints (or unicode decomposed codepoints) from
  // the input text.
  std::set<UChar32> codepoints;
  RetrieveCodepointsAndDecomposedCodepoints(text, &codepoints);

  // Determine which fallback font is given the fewer missing glyphs.
  for (UChar32 codepoint : codepoints) {
    sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
        template_font.GetFontName().c_str(), skia_style, locales, num_locales,
        codepoint));
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
