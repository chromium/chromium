// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"

#include <algorithm>
#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/font_fallback_skia_impl.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

namespace {

// Queries the registry to get a mapping from font filenames to font names.
void QueryFontsFromRegistry(std::map<std::string, std::string>* map) {
  const wchar_t* kFonts =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

  base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE, kFonts);
  for (; it.Valid(); ++it) {
    const std::string filename =
        base::ToLowerASCII(base::WideToUTF8(it.Value()));
    (*map)[filename] = base::WideToUTF8(it.Name());
  }
}

// Fills |font_names| with a list of font families found in the font file at
// |filename|. Takes in a |font_map| from font filename to font families, which
// is filled-in by querying the registry, if empty.
void GetFontNamesFromFilename(const std::string& filename,
                              std::map<std::string, std::string>* font_map,
                              std::vector<std::string>* font_names) {
  if (font_map->empty())
    QueryFontsFromRegistry(font_map);

  std::map<std::string, std::string>::const_iterator it =
      font_map->find(base::ToLowerASCII(filename));
  if (it == font_map->end())
    return;

  internal::ParseFontFamilyString(it->second, font_names);
}

// Returns true if |text| contains only ASCII digits.
bool ContainsOnlyDigits(const std::string& text) {
  return text.find_first_not_of("0123456789") == base::string16::npos;
}

// Appends a Font with the given |name| and |size| to |fonts| unless the last
// entry is already a font with that name.
void AppendFont(const std::string& name, int size, std::vector<Font>* fonts) {
  if (fonts->empty() || fonts->back().GetFontName() != name)
    fonts->push_back(Font(name, size));
}

// Queries the registry to get a list of linked fonts for |font|.
void QueryLinkedFontsFromRegistry(const Font& font,
                                  std::map<std::string, std::string>* font_map,
                                  std::vector<Font>* linked_fonts) {
  std::string logging_str;
  const wchar_t* kSystemLink =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink";

  base::win::RegKey key;
  if (FAILED(key.Open(HKEY_LOCAL_MACHINE, kSystemLink, KEY_READ)))
    return;

  const std::wstring original_font_name = base::UTF8ToWide(font.GetFontName());
  std::vector<std::wstring> values;
  if (FAILED(key.ReadValues(original_font_name.c_str(), &values))) {
    key.Close();
    return;
  }

  base::StringAppendF(&logging_str, "Original font: %s\n",
                      font.GetFontName().c_str());

  std::string filename;
  std::string font_name;
  for (size_t i = 0; i < values.size(); ++i) {
    internal::ParseFontLinkEntry(
        base::WideToUTF8(values[i]), &filename, &font_name);

    base::StringAppendF(&logging_str, "fallback: '%s' '%s'\n",
                        font_name.c_str(), filename.c_str());

    // If the font name is present, add that directly, otherwise add the
    // font names corresponding to the filename.
    if (!font_name.empty()) {
      AppendFont(font_name, font.GetFontSize(), linked_fonts);
    } else if (!filename.empty()) {
      std::vector<std::string> filename_fonts;
      GetFontNamesFromFilename(filename, font_map, &filename_fonts);
      for (const std::string& filename_font : filename_fonts)
        AppendFont(filename_font, font.GetFontSize(), linked_fonts);
    }
  }

  key.Close();

  for (const auto& resolved_font : *linked_fonts) {
    base::StringAppendF(&logging_str, "resolved: '%s'\n",
                        resolved_font.GetFontName().c_str());
  }

  TRACE_EVENT1("fonts", "QueryLinkedFontsFromRegistry", "results", logging_str);
}

// CachedFontLinkSettings is a singleton cache of the Windows font settings
// from the registry. It maintains a cached view of the registry's list of
// system fonts and their font link chains.
class CachedFontLinkSettings {
 public:
  static CachedFontLinkSettings* GetInstance();

  // Returns the linked fonts list correspond to |font|. Returned value will
  // never be null.
  const std::vector<Font>* GetLinkedFonts(const Font& font);

 private:
  friend struct base::DefaultSingletonTraits<CachedFontLinkSettings>;

  CachedFontLinkSettings();
  virtual ~CachedFontLinkSettings();

  // Map of system fonts, from file names to font families.
  std::map<std::string, std::string> cached_system_fonts_;

  // Map from font names to vectors of linked fonts.
  std::map<std::string, std::vector<Font> > cached_linked_fonts_;

  DISALLOW_COPY_AND_ASSIGN(CachedFontLinkSettings);
};

// static
CachedFontLinkSettings* CachedFontLinkSettings::GetInstance() {
  return base::Singleton<
      CachedFontLinkSettings,
      base::LeakySingletonTraits<CachedFontLinkSettings>>::get();
}

const std::vector<Font>* CachedFontLinkSettings::GetLinkedFonts(
    const Font& font) {
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("FontFallback.GetLinkedFonts.Timing");
  const std::string& font_name = font.GetFontName();
  std::map<std::string, std::vector<Font> >::const_iterator it =
      cached_linked_fonts_.find(font_name);
  if (it != cached_linked_fonts_.end())
    return &it->second;

  TRACE_EVENT1("fonts", "CachedFontLinkSettings::GetLinkedFonts", "font_name",
               font_name);

  SCOPED_UMA_HISTOGRAM_LONG_TIMER(
      "FontFallback.GetLinkedFonts.CacheMissTiming");
  std::vector<Font>* linked_fonts = &cached_linked_fonts_[font_name];
  QueryLinkedFontsFromRegistry(font, &cached_system_fonts_, linked_fonts);
  UMA_HISTOGRAM_COUNTS_100("FontFallback.GetLinkedFonts.FontCount",
                           linked_fonts->size());
  return linked_fonts;
}

CachedFontLinkSettings::CachedFontLinkSettings() {
}

CachedFontLinkSettings::~CachedFontLinkSettings() {
}

}  // namespace

namespace internal {

void ParseFontLinkEntry(const std::string& entry,
                        std::string* filename,
                        std::string* font_name) {
  std::vector<std::string> parts = base::SplitString(
      entry, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  filename->clear();
  font_name->clear();
  if (parts.size() > 0)
    *filename = parts[0];
  // The second entry may be the font name or the first scaling factor, if the
  // entry does not contain a font name. If it contains only digits, assume it
  // is a scaling factor.
  if (parts.size() > 1 && !ContainsOnlyDigits(parts[1]))
    *font_name = parts[1];
}

void ParseFontFamilyString(const std::string& family,
                           std::vector<std::string>* font_names) {
  // The entry is comma separated, having the font filename as the first value
  // followed optionally by the font family name and a pair of integer scaling
  // factors.
  // TODO(asvitkine): Should we support these scaling factors?
  *font_names = base::SplitString(family, "&", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  if (!font_names->empty()) {
    const size_t index = font_names->back().find('(');
    if (index != std::string::npos) {
      font_names->back().resize(index);
      base::TrimWhitespaceASCII(font_names->back(), base::TRIM_TRAILING,
                                &font_names->back());
    }
  }
}

}  // namespace internal

std::vector<Font> GetFallbackFonts(const Font& font) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFonts");
  std::string font_family = font.GetFontName();
  CachedFontLinkSettings* font_link = CachedFontLinkSettings::GetInstance();
  // GetLinkedFonts doesn't care about the font size, so we always pass 10.
  return *font_link->GetLinkedFonts(Font(font_family, 10));
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");
  // Creating a DirectWrite font fallback can be expensive. It's ok in the
  // browser process because we can use the shared system fallback, but in the
  // renderer this can cause hangs. Code that needs font fallback in the
  // renderer should instead use the font proxy.
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  // The text passed must be at least length 1.
  if (text.empty())
    return false;

  // Check that we have at least as much text as was claimed. If we have less
  // text than expected then DirectWrite will become confused and crash. This
  // shouldn't happen, but crbug.com/624905 shows that it happens sometimes.
  constexpr base::char16 kNulCharacter = '\0';
  if (text.find(kNulCharacter) != base::StringPiece16::npos)
    return false;

  sk_sp<SkTypeface> fallback_typeface =
      GetSkiaFallbackTypeface(font, locale, text);

  if (!fallback_typeface)
    return false;

  // Fallback needs to keep the exact SkTypeface, as re-matching the font using
  // family name and styling information loses access to the underlying platform
  // font handles and is not guaranteed to result in the correct typeface, see
  // https://crbug.com/1003829
  *result = Font(PlatformFont::CreateFromSkTypeface(
      std::move(fallback_typeface), font.GetFontSize(), base::nullopt));
  return true;
}

}  // namespace gfx
