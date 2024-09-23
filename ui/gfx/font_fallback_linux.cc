// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/font_fallback_linux.h"

#include <fontconfig/fontconfig.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/linux/fontconfig_util.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

namespace {

const char kFontFormatTrueType[] = "TrueType";
const char kFontFormatCFF[] = "CFF";

bool IsValidFontFromPattern(FcPattern* pattern) {
  // Ignore any bitmap fonts users may still have installed from last
  // century.
  if (!IsFontScalable(pattern))
    return false;

  // Take only supported font formats on board.
  std::string format = GetFontFormat(pattern);
  if (format != kFontFormatTrueType && format != kFontFormatCFF)
    return false;

  // Ignore any fonts FontConfig knows about, but that we don't have
  // permission to read.
  base::FilePath font_path = GetFontPath(pattern);
  if (font_path.empty() || access(font_path.AsUTF8Unsafe().c_str(), R_OK))
    return false;

  return true;
}

// This class uniquely identified a typeface. A typeface can be identified by
// its file path and it's ttc index.
class TypefaceCacheKey {
 public:
  TypefaceCacheKey(const base::FilePath& font_path, int ttc_index)
      : font_path_(font_path), ttc_index_(ttc_index) {}
  TypefaceCacheKey(const TypefaceCacheKey&) = default;
  TypefaceCacheKey& operator=(const TypefaceCacheKey&) = default;

  const base::FilePath& font_path() const { return font_path_; }
  int ttc_index() const { return ttc_index_; }

  bool operator<(const TypefaceCacheKey& other) const {
    return std::tie(ttc_index_, font_path_) <
           std::tie(other.ttc_index_, other.font_path_);
  }

 private:
  base::FilePath font_path_;
  int ttc_index_;
};

// Returns a SkTypeface for a given font path and ttc_index. The typeface is
// cached to avoid reloading the font from file. SkTypeface is not caching
// these requests.
sk_sp<SkTypeface> GetSkTypefaceFromPathAndIndex(const base::FilePath& font_path,
                                                int ttc_index) {
  using TypefaceCache = std::map<TypefaceCacheKey, sk_sp<SkTypeface>>;
  static base::NoDestructor<TypefaceCache> typeface_cache;

  if (font_path.empty())
    return nullptr;

  TypefaceCache* cache = typeface_cache.get();
  TypefaceCacheKey key(font_path, ttc_index);
  TypefaceCache::iterator entry = cache->find(key);
  if (entry != cache->end())
    return sk_sp<SkTypeface>(entry->second);

  sk_sp<SkFontMgr> font_mgr = skia::DefaultFontMgr();
  std::string filename = font_path.AsUTF8Unsafe();
  sk_sp<SkTypeface> typeface =
      font_mgr->makeFromFile(filename.c_str(), ttc_index);
  (*cache)[key] = typeface;

  return sk_sp<SkTypeface>(typeface);
}

// Implements a fallback font cache over FontConfig API.
//
// A MRU cache is kept from a font to its potential fallback fonts.
// The key (e.g. FallbackFontEntry) contains the font for which
// fallback font must be returned.
//
// For each key, the cache is keeping a set (e.g. FallbackFontEntries) of
// potential fallback font (e.g. FallbackFontEntry). Each fallback font entry
// contains the supported codepoints (e.g. charset). The fallback font returned
// by GetFallbackFont(...) depends on the input text and is using the charset
// to determine the best candidate.
class FallbackFontKey {
 public:
  FallbackFontKey(std::string locale, Font font)
      : locale_(locale), font_(font) {}

  FallbackFontKey(const FallbackFontKey&) = default;

  FallbackFontKey& operator=(const FallbackFontKey&) = delete;

  ~FallbackFontKey() = default;

  bool operator<(const FallbackFontKey& other) const {
    if (font_.GetFontSize() != other.font_.GetFontSize())
      return font_.GetFontSize() < other.font_.GetFontSize();
    if (font_.GetStyle() != other.font_.GetStyle())
      return font_.GetStyle() < other.font_.GetStyle();
    if (font_.GetFontName() != other.font_.GetFontName())
      return font_.GetFontName() < other.font_.GetFontName();
    return locale_ < other.locale_;
  }

 private:
  std::string locale_;
  Font font_;
};

class FallbackFontEntry {
 public:
  FallbackFontEntry(const base::FilePath& font_path,
                    int ttc_index,
                    FontRenderParams font_params,
                    FcCharSet* charset)
      : font_path_(font_path),
        ttc_index_(ttc_index),
        font_params_(font_params),
        charset_(FcCharSetCopy(charset)) {}

  FallbackFontEntry(const FallbackFontEntry& other)
      : font_path_(other.font_path_),
        ttc_index_(other.ttc_index_),
        font_params_(other.font_params_),
        charset_(FcCharSetCopy(other.charset_)) {}

  FallbackFontEntry& operator=(const FallbackFontEntry&) = delete;

  ~FallbackFontEntry() { FcCharSetDestroy(charset_); }

  const base::FilePath& font_path() const { return font_path_; }
  int ttc_index() const { return ttc_index_; }
  FontRenderParams font_params() const { return font_params_; }

  // Returns whether the fallback font support the codepoint.
  bool HasGlyphForCharacter(UChar32 c) const {
    return FcCharSetHasChar(charset_, static_cast<FcChar32>(c));
  }

 private:
  // Font identity fields.
  base::FilePath font_path_;
  int ttc_index_;

  // Font rendering parameters.
  FontRenderParams font_params_;

  // Font code points coverage.
  raw_ptr<FcCharSet> charset_;
};

using FallbackFontEntries = std::vector<FallbackFontEntry>;
using FallbackFontEntriesCache =
    base::LRUCache<FallbackFontKey, FallbackFontEntries>;

// The fallback font cache is a mapping from a font to the potential fallback
// fonts with their codepoint coverage.
FallbackFontEntriesCache* GetFallbackFontEntriesCacheInstance() {
  constexpr int kFallbackFontCacheSize = 256;
  static base::NoDestructor<FallbackFontEntriesCache> cache(
      kFallbackFontCacheSize);
  return cache.get();
}

// The fallback fonts cache is a mapping from a font family name to its
// potential fallback fonts.
using FallbackFontList = std::vector<Font>;
using FallbackFontListCache = base::LRUCache<std::string, FallbackFontList>;

FallbackFontListCache* GetFallbackFontListCacheInstance() {
  constexpr int kFallbackCacheSize = 64;
  static base::NoDestructor<FallbackFontListCache> fallback_cache(
      kFallbackCacheSize);
  return fallback_cache.get();
}

}  // namespace

size_t GetFallbackFontEntriesCacheSizeForTesting() {
  return GetFallbackFontEntriesCacheInstance()->size();
}

size_t GetFallbackFontListCacheSizeForTesting() {
  return GetFallbackFontListCacheInstance()->size();
}

void ClearAllFontFallbackCachesForTesting() {
  GetFallbackFontEntriesCacheInstance()->Clear();
  GetFallbackFontListCacheInstance()->Clear();
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     std::u16string_view text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  // The text passed must be at least length 1.
  if (text.empty())
    return false;

  FallbackFontEntriesCache* cache = GetFallbackFontEntriesCacheInstance();
  FallbackFontKey key(locale, font);
  FallbackFontEntriesCache::iterator cache_entry = cache->Get(key);

  // The cache entry for this font is missing, build it.
  if (cache_entry == cache->end()) {
    ScopedFcPattern pattern(FcPatternCreate());

    // Add pattern for family name.
    std::string font_family = font.GetFontName();
    FcPatternAddString(pattern.get(), FC_FAMILY,
                       reinterpret_cast<const FcChar8*>(font_family.c_str()));

    // Prefer scalable font.
    FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);

    // Add pattern for locale.
    FcPatternAddString(pattern.get(), FC_LANG,
                       reinterpret_cast<const FcChar8*>(locale.c_str()));

    // Add pattern for font style.
    if ((font.GetStyle() & gfx::Font::ITALIC) != 0)
      FcPatternAddInteger(pattern.get(), FC_SLANT, FC_SLANT_ITALIC);

    // Match a font fallback.
    FcConfig* config = GetGlobalFontConfig();
    FcConfigSubstitute(config, pattern.get(), FcMatchPattern);
    FcDefaultSubstitute(pattern.get());

    FallbackFontEntries fallback_font_entries;
    FcResult fc_result;
    FcFontSet* fonts =
        FcFontSort(config, pattern.get(), FcTrue, nullptr, &fc_result);
    if (fonts) {
      // Add each potential fallback font returned by font-config to the
      // set of fallback fonts and keep track of their codepoints coverage.
      for (int i = 0; i < fonts->nfont; ++i) {
        FcPattern* current_font = fonts->fonts[i];
        if (!IsValidFontFromPattern(current_font))
          continue;

        // Retrieve the font identity fields.
        base::FilePath font_path = GetFontPath(current_font);
        int font_ttc_index = GetFontTtcIndex(current_font);

        // Retrieve the charset of the current font.
        FcCharSet* char_set = nullptr;
        fc_result = FcPatternGetCharSet(current_font, FC_CHARSET, 0, &char_set);
        if (fc_result != FcResultMatch || char_set == nullptr)
          continue;

        // Retrieve the font render params.
        FontRenderParams font_params;
        GetFontRenderParamsFromFcPattern(current_font, &font_params);

        fallback_font_entries.push_back(FallbackFontEntry(
            font_path, font_ttc_index, font_params, char_set));
      }
      FcFontSetDestroy(fonts);
    }

    cache_entry = cache->Put(key, std::move(fallback_font_entries));
  }

  // Try each font in the cache to find the one with the highest coverage.
  size_t fewest_missing_glyphs = text.length() + 1;
  const FallbackFontEntry* prefered_entry = nullptr;

  for (const auto& entry : cache_entry->second) {
    // Validate that every character has a known glyph in the font.
    size_t missing_glyphs = 0;
    size_t matching_glyphs = 0;
    size_t i = 0;
    while (i < text.length()) {
      UChar32 c = 0;
      U16_NEXT(text.data(), i, text.length(), c);
      if (entry.HasGlyphForCharacter(c)) {
        ++matching_glyphs;
      } else {
        ++missing_glyphs;
      }
    }

    if (matching_glyphs > 0 && missing_glyphs < fewest_missing_glyphs) {
      fewest_missing_glyphs = missing_glyphs;
      prefered_entry = &entry;
    }

    // The font has coverage for the given text and is a valid fallback font.
    if (missing_glyphs == 0)
      break;
  }

  // No fonts can be used as font fallback.
  if (!prefered_entry)
    return false;

  sk_sp<SkTypeface> typeface = GetSkTypefaceFromPathAndIndex(
      prefered_entry->font_path(), prefered_entry->ttc_index());
  // The file can't be parsed (e.g. corrupt). This font can't be used as a
  // fallback font.
  if (!typeface)
    return false;

  Font fallback_font(PlatformFont::CreateFromSkTypeface(
      typeface, font.GetFontSize(), prefered_entry->font_params()));

  *result = fallback_font;
  return true;
}

std::vector<Font> GetFallbackFonts(const Font& font) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFonts");

  std::string font_family = font.GetFontName();

  // Lookup in the cache for already processed family.
  FallbackFontListCache* font_cache = GetFallbackFontListCacheInstance();
  auto cached_fallback_fonts = font_cache->Get(font_family);
  if (cached_fallback_fonts != font_cache->end()) {
    // Already in cache.
    return cached_fallback_fonts->second;
  }

  // Retrieve the font fallbacks for a given family name.
  FallbackFontList fallback_fonts;
  FcPattern* pattern = FcPatternCreate();
  FcPatternAddString(pattern, FC_FAMILY,
                     reinterpret_cast<const FcChar8*>(font_family.c_str()));

  FcConfig* config = GetGlobalFontConfig();
  if (FcConfigSubstitute(config, pattern, FcMatchPattern) == FcTrue) {
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcFontSet* fonts = FcFontSort(config, pattern, FcTrue, nullptr, &result);
    if (fonts) {
      std::set<std::string> fallback_names;
      for (int i = 0; i < fonts->nfont; ++i) {
        std::string name_str = GetFontName(fonts->fonts[i]);
        if (name_str.empty())
          continue;

        // FontConfig returns multiple fonts with the same family name and
        // different configurations. Check to prevent duplicate family names.
        if (fallback_names.insert(name_str).second)
          fallback_fonts.push_back(Font(name_str, 13));
      }
      FcFontSetDestroy(fonts);
    }
  }
  FcPatternDestroy(pattern);

  // Store the font fallbacks to the cache.
  font_cache->Put(font_family, fallback_fonts);

  return fallback_fonts;
}

namespace {

class CachedFont {
 public:
  // Note: We pass the charset explicitly as callers
  // should not create CachedFont entries without knowing
  // that the FcPattern contains a valid charset.
  CachedFont(FcPattern* pattern, FcCharSet* char_set)
      : supported_characters_(char_set) {
    DCHECK(pattern);
    DCHECK(char_set);
    fallback_font_.name = GetFontName(pattern);
    fallback_font_.filepath = GetFontPath(pattern);
    fallback_font_.ttc_index = GetFontTtcIndex(pattern);
    fallback_font_.is_bold = IsFontBold(pattern);
    fallback_font_.is_italic = IsFontItalic(pattern);
  }

  const FallbackFontData& fallback_font() const { return fallback_font_; }

  bool HasGlyphForCharacter(UChar32 c) const {
    return supported_characters_ && FcCharSetHasChar(supported_characters_, c);
  }

 private:
  FallbackFontData fallback_font_;
  // supported_characters_ is owned by the parent
  // FcFontSet and should never be freed.
  raw_ptr<FcCharSet> supported_characters_;
};

class CachedFontSet {
 public:
  // CachedFontSet takes ownership of the passed FcFontSet.
  static std::unique_ptr<CachedFontSet> CreateForLocale(
      const std::string& locale) {
    FcFontSet* font_set = CreateFcFontSetForLocale(locale);
    return base::WrapUnique(new CachedFontSet(font_set));
  }

  CachedFontSet(const CachedFontSet&) = delete;
  CachedFontSet& operator=(const CachedFontSet&) = delete;

  ~CachedFontSet() {
    fallback_list_.clear();
    FcFontSetDestroy(font_set_);
  }

  bool GetFallbackFontForChar(UChar32 c, FallbackFontData* fallback_font) {
    TRACE_EVENT0("fonts", "gfx::CachedFontSet::GetFallbackFontForChar");

    for (const auto& cached_font : fallback_list_) {
      if (cached_font.HasGlyphForCharacter(c)) {
        *fallback_font = cached_font.fallback_font();
        return true;
      }
    }
    return false;
  }

 private:
  static FcFontSet* CreateFcFontSetForLocale(const std::string& locale) {
    FcPattern* pattern = FcPatternCreate();

    if (!locale.empty()) {
      // FcChar* is unsigned char* so we have to cast.
      FcPatternAddString(pattern, FC_LANG,
                         reinterpret_cast<const FcChar8*>(locale.c_str()));
    }

    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    if (locale.empty())
      FcPatternDel(pattern, FC_LANG);

    // The result parameter returns if any fonts were found.
    // We already handle 0 fonts correctly, so we ignore the param.
    FcResult result;
    FcFontSet* font_set = FcFontSort(0, pattern, 0, 0, &result);
    FcPatternDestroy(pattern);

    // The caller will take ownership of this FcFontSet.
    return font_set;
  }

  CachedFontSet(FcFontSet* font_set) : font_set_(font_set) {
    FillFallbackList();
  }

  void FillFallbackList() {
    TRACE_EVENT0("fonts", "gfx::CachedFontSet::FillFallbackList");

    DCHECK(fallback_list_.empty());
    if (!font_set_)
      return;

    for (int i = 0; i < font_set_->nfont; ++i) {
      FcPattern* pattern = font_set_->fonts[i];

      if (!IsValidFontFromPattern(pattern))
        continue;

      // Make sure this font can tell us what characters it has glyphs for.
      FcCharSet* char_set;
      if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &char_set) !=
          FcResultMatch)
        continue;

      fallback_list_.emplace_back(pattern, char_set);
    }
  }

  raw_ptr<FcFontSet> font_set_;  // Owned by this object.
  // CachedFont has a FcCharset* which points into the FcFontSet.
  // If the FcFontSet is ever destroyed, the fallback list
  // must be cleared first.
  std::vector<CachedFont> fallback_list_;
};

typedef std::map<std::string, std::unique_ptr<CachedFontSet>> FontSetCache;
base::LazyInstance<FontSetCache>::Leaky g_font_sets_by_locale =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FallbackFontData::FallbackFontData() = default;
FallbackFontData::FallbackFontData(const FallbackFontData& other) = default;
FallbackFontData& FallbackFontData::operator=(const FallbackFontData& other) =
    default;

bool GetFallbackFontForChar(UChar32 c,
                            const std::string& locale,
                            FallbackFontData* fallback_font) {
  auto& cached_font_set = g_font_sets_by_locale.Get()[locale];
  if (!cached_font_set)
    cached_font_set = CachedFontSet::CreateForLocale(locale);
  return cached_font_set->GetFallbackFontForChar(c, fallback_font);
}

}  // namespace gfx
