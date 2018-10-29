// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"

#include <dwrite_2.h>
#include <usp10.h>
#include <wrl.h>
#include <wrl/client.h>

#include <algorithm>
#include <map>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/text_analysis_source.h"

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
      std::vector<std::string> font_names;
      GetFontNamesFromFilename(filename, font_map, &font_names);
      for (size_t i = 0; i < font_names.size(); ++i)
        AppendFont(font_names[i], font.GetFontSize(), linked_fonts);
    }
  }

  key.Close();

  for (const auto& resolved_font : *linked_fonts) {
    base::StringAppendF(&logging_str, "resolved: '%s'\n",
                        resolved_font.GetFontName().c_str());
  }

  TRACE_EVENT1("ui", "QueryLinkedFontsFromRegistry", "results", logging_str);
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

  TRACE_EVENT1("ui", "CachedFontLinkSettings::GetLinkedFonts", "font_name",
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

// Callback to |EnumEnhMetaFile()| to intercept font creation.
int CALLBACK MetaFileEnumProc(HDC hdc,
                              HANDLETABLE* table,
                              CONST ENHMETARECORD* record,
                              int table_entries,
                              LPARAM log_font) {
  if (record->iType == EMR_EXTCREATEFONTINDIRECTW) {
    const EMREXTCREATEFONTINDIRECTW* create_font_record =
        reinterpret_cast<const EMREXTCREATEFONTINDIRECTW*>(record);
    *reinterpret_cast<LOGFONT*>(log_font) = create_font_record->elfw.elfLogFont;
  }
  return 1;
}

bool GetUniscribeFallbackFont(const Font& font,
                              const wchar_t* text,
                              int text_length,
                              Font* result) {
  // Adapted from WebKit's |FontCache::GetFontDataForCharacters()|.
  // Uniscribe doesn't expose a method to query fallback fonts, so this works by
  // drawing the text to an EMF object with Uniscribe's ScriptStringOut and then
  // inspecting the EMF object to figure out which font Uniscribe used.
  //
  // DirectWrite in Windows 8.1 provides a cleaner alternative:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dn280480.aspx

  static HDC hdc = CreateCompatibleDC(NULL);

  // Use a meta file to intercept the fallback font chosen by Uniscribe.
  HDC meta_file_dc = CreateEnhMetaFile(hdc, NULL, NULL, NULL);
  if (!meta_file_dc)
    return false;

  SelectObject(meta_file_dc, font.GetNativeFont());

  SCRIPT_STRING_ANALYSIS script_analysis;
  HRESULT hresult =
      ScriptStringAnalyse(meta_file_dc, text, text_length, 0, -1,
                          SSA_METAFILE | SSA_FALLBACK | SSA_GLYPHS | SSA_LINK,
                          0, NULL, NULL, NULL, NULL, NULL, &script_analysis);

  if (SUCCEEDED(hresult)) {
    hresult = ScriptStringOut(script_analysis, 0, 0, 0, NULL, 0, 0, FALSE);
    ScriptStringFree(&script_analysis);
  }

  bool found_fallback = false;
  HENHMETAFILE meta_file = CloseEnhMetaFile(meta_file_dc);
  if (SUCCEEDED(hresult)) {
    LOGFONT log_font;
    log_font.lfFaceName[0] = 0;
    EnumEnhMetaFile(0, meta_file, MetaFileEnumProc, &log_font, NULL);
    if (log_font.lfFaceName[0]) {
      *result =
          Font(base::UTF16ToUTF8(log_font.lfFaceName), font.GetFontSize());
      found_fallback = true;
    }
  }
  DeleteEnhMetaFile(meta_file);

  return found_fallback;
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
  *font_names = base::SplitString(
      family, "&", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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
  std::string font_family = font.GetFontName();
  CachedFontLinkSettings* font_link = CachedFontLinkSettings::GetInstance();
  // GetLinkedFonts doesn't care about the font size, so we always pass 10.
  return *font_link->GetLinkedFonts(Font(font_family, 10));
}

bool GetFallbackFont(const Font& font,
                     const base::char16* text,
                     int text_length,
                     Font* result) {
  // Creating a DirectWrite font fallback can be expensive. It's ok in the
  // browser process because we can use the shared system fallback, but in the
  // renderer this can cause hangs. Code that needs font fallback in the
  // renderer should instead use the font proxy.
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // Check that we have at least as much text as was claimed. If we have less
  // text than expected then DirectWrite will become confused and crash. This
  // shouldn't happen, but crbug.com/624905 shows that it happens sometimes.
  DCHECK_GE(wcslen(text), static_cast<size_t>(text_length));
  text_length = std::min(wcslen(text), static_cast<size_t>(text_length));

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(factory.GetAddressOf());
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
  factory.CopyTo(factory2.GetAddressOf());
  if (!factory2) {
    // IDWriteFactory2 is not available before Win8.1
    return GetUniscribeFallbackFont(font, text, text_length, result);
  }

  Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback;
  if (FAILED(factory2->GetSystemFontFallback(fallback.GetAddressOf())))
    return false;

  base::string16 locale = base::UTF8ToUTF16(base::i18n::GetConfiguredLocale());

  Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> number_substitution;
  if (FAILED(factory2->CreateNumberSubstitution(
          DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, locale.c_str(),
          true /* ignoreUserOverride */, number_substitution.GetAddressOf()))) {
    return false;
  }

  uint32_t mapped_length = 0;
  Microsoft::WRL::ComPtr<IDWriteFont> mapped_font;
  float scale = 0;
  Microsoft::WRL::ComPtr<IDWriteTextAnalysisSource> text_analysis;
  DWRITE_READING_DIRECTION reading_direction =
      base::i18n::IsRTL() ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
                          : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
  if (FAILED(gfx::win::TextAnalysisSource::Create(
          text_analysis.GetAddressOf(), text, locale.c_str(),
          number_substitution.Get(), reading_direction))) {
    return false;
  }
  base::string16 original_name = base::UTF8ToUTF16(font.GetFontName());
  DWRITE_FONT_STYLE font_style = DWRITE_FONT_STYLE_NORMAL;
  if (font.GetStyle() & Font::ITALIC)
    font_style = DWRITE_FONT_STYLE_ITALIC;
  if (FAILED(fallback->MapCharacters(
          text_analysis.Get(), 0, text_length, nullptr, original_name.c_str(),
          static_cast<DWRITE_FONT_WEIGHT>(font.GetWeight()), font_style,
          DWRITE_FONT_STRETCH_NORMAL, &mapped_length,
          mapped_font.GetAddressOf(), &scale))) {
    return false;
  }

  if (mapped_font) {
    base::string16 name;
    if (FAILED(GetFamilyNameFromDirectWriteFont(mapped_font.Get(), &name)))
      return false;
    *result = Font(base::UTF16ToUTF8(name), font.GetFontSize() * scale);
    return true;
  }
  return false;
}

}  // namespace gfx
