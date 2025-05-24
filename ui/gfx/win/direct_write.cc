// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_write.h"

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <wrl.h>
#include <wrl/client.h>

#include <string>
#include <string_view>

#include "base/debug/alias.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace gfx {
namespace win {

namespace {

// Pointer to the global IDWriteFactory interface.
IDWriteFactory* g_direct_write_factory = nullptr;

// Used in tests to allow a known font to masquerade as a locally installed
// font. Usually this is the Ahem.ttf font. Leaked at shutdown.
std::vector<base::FilePath>* g_sideloaded_fonts = nullptr;

bool GetFontCollection(Microsoft::WRL::ComPtr<IDWriteFactory>& factory,
                       IDWriteFontCollection** collection) {
  if (!g_sideloaded_fonts) {
    // Normal path - The DirectWrite font manager will use the system's font
    // collection with no sideloading when passed `nullptr` for the collection.
    return false;
  }

  // QueryInterface for IDWriteFactory2. This should succeed since we only
  // support >= Win10.
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
  factory.As<IDWriteFactory2>(&factory2);
  DCHECK(factory2);

  // QueryInterface for IDwriteFactory3, needed for MatchUniqueFont on Windows.
  // This should succeed since we only support >= Win10.
  Microsoft::WRL::ComPtr<IDWriteFactory3> factory3;
  factory2.As<IDWriteFactory3>(&factory3);
  DCHECK(factory3);

  // If sideloading - build a font set with sideloads then add the system font
  // collection.
  Microsoft::WRL::ComPtr<IDWriteFontSetBuilder> font_set_builder;
  HRESULT hr = factory3->CreateFontSetBuilder(&font_set_builder);
  DCHECK(SUCCEEDED(hr));

  for (auto& path : *g_sideloaded_fonts) {
    Microsoft::WRL::ComPtr<IDWriteFontFile> font_file;
    hr = factory3->CreateFontFileReference(path.value().c_str(), nullptr,
                                           &font_file);
    DCHECK(SUCCEEDED(hr));

    BOOL supported;
    DWRITE_FONT_FILE_TYPE file_type;
    UINT32 n_fonts;
    hr = font_file->Analyze(&supported, &file_type, nullptr, &n_fonts);
    DCHECK(SUCCEEDED(hr));

    for (UINT32 font_index = 0; font_index < n_fonts; ++font_index) {
      Microsoft::WRL::ComPtr<IDWriteFontFaceReference> font_face;
      hr = factory3->CreateFontFaceReference(font_file.Get(), font_index,
                                             DWRITE_FONT_SIMULATIONS_NONE,
                                             &font_face);
      DCHECK(SUCCEEDED(hr));

      hr = font_set_builder->AddFontFaceReference(font_face.Get());
      DCHECK(SUCCEEDED(hr));
    }
  }
  // Now add the system fonts.
  Microsoft::WRL::ComPtr<IDWriteFontSet> system_font_set;
  hr = factory3->GetSystemFontSet(&system_font_set);
  DCHECK(SUCCEEDED(hr));

  hr = font_set_builder->AddFontSet(system_font_set.Get());
  DCHECK(SUCCEEDED(hr));

  // Make the set.
  Microsoft::WRL::ComPtr<IDWriteFontSet> font_set;
  hr = font_set_builder->CreateFontSet(&font_set);
  DCHECK(SUCCEEDED(hr));

  // Make the collection.
  Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection1;
  hr = factory3->CreateFontCollectionFromFontSet(font_set.Get(), &collection1);
  DCHECK(SUCCEEDED(hr));

  hr = collection1->QueryInterface(collection);
  DCHECK(SUCCEEDED(hr));

  return true;
}

void SetDirectWriteFactory(IDWriteFactory* factory) {
  DCHECK(!g_direct_write_factory);
  // We grab a reference on the DirectWrite factory. This reference is
  // leaked, which is ok because skia leaks it as well.
  factory->AddRef();
  g_direct_write_factory = factory;
}

}  // anonymous namespace

void SideLoadFontForTesting(base::FilePath path) {
  if (!g_sideloaded_fonts) {
    // Note: this list is leaked.
    g_sideloaded_fonts = new std::vector<base::FilePath>();
  }
  g_sideloaded_fonts->push_back(path);
}

void CreateDWriteFactory(IDWriteFactory** factory) {
  Microsoft::WRL::ComPtr<IUnknown> factory_unknown;
  HRESULT hr =
      DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                          &factory_unknown);
  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    NOTREACHED();
  }
  factory_unknown.CopyTo(factory);
}

void InitializeDirectWrite() {
  static bool tried_dwrite_initialize = false;
  DCHECK(!tried_dwrite_initialize);
  tried_dwrite_initialize = true;

  TRACE_EVENT0("fonts", "gfx::InitializeDirectWrite");

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  CreateDWriteFactory(&factory);
  CHECK(!!factory);
  SetDirectWriteFactory(factory.Get());

  // Get a font collection that contains sideloaded fonts for web tests, or
  // nullptr to tell the DirectWrite FontMgr to use the system font collection.
  // SkFontMgr_DirectWrite increments this object's ref count.
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
  bool should_use_collection = GetFontCollection(factory, &collection);
  if (g_sideloaded_fonts) {
    DCHECK(should_use_collection);
  } else {
    DCHECK(!should_use_collection);
  }

  sk_sp<SkFontMgr> direct_write_font_mgr = SkFontMgr_New_DirectWrite(
      factory.Get(), should_use_collection ? collection.Get() : nullptr);
  CHECK(!!direct_write_font_mgr);

  // Override the default skia font manager. This must be called before any
  // use of the skia font manager is done (e.g. before any call to
  // skia::DefaultFontMgr()).
  skia::OverrideDefaultSkFontMgr(std::move(direct_write_font_mgr));
}

IDWriteFactory* GetDirectWriteFactory() {
  // Some unittests may access this accessor without any previous call to
  // |InitializeDirectWrite|. A call to |InitializeDirectWrite| after this
  // function being called is still invalid.
  if (!g_direct_write_factory)
    InitializeDirectWrite();
  return g_direct_write_factory;
}

std::optional<std::string> RetrieveLocalizedString(
    IDWriteLocalizedStrings* names,
    const std::string& locale) {
  std::wstring locale_wide = base::UTF8ToWide(locale);

  // If locale is empty, index 0 will be used. Otherwise, the locale name must
  // be found and must exist.
  UINT32 index = 0;
  BOOL exists = false;
  if (!locale.empty() &&
      (FAILED(names->FindLocaleName(locale_wide.c_str(), &index, &exists)) ||
       !exists)) {
    return std::nullopt;
  }

  // Get the string length.
  UINT32 length = 0;
  if (FAILED(names->GetStringLength(index, &length)))
    return std::nullopt;

  // The output buffer length needs to be one larger to receive the NUL
  // character.
  std::wstring buffer;
  buffer.resize(length + 1);
  if (FAILED(names->GetString(index, &buffer[0], buffer.size())))
    return std::nullopt;

  // Shrink the string to fit the actual length.
  buffer.resize(length);

  return base::WideToUTF8(buffer);
}

std::optional<std::string> RetrieveLocalizedFontName(
    std::string_view font_name,
    const std::string& locale) {
  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  CreateDWriteFactory(&factory);

  Microsoft::WRL::ComPtr<IDWriteFontCollection> font_collection;
  if (FAILED(factory->GetSystemFontCollection(&font_collection))) {
    return std::nullopt;
  }

  UINT32 index = 0;
  BOOL exists;
  std::wstring font_name_wide = base::UTF8ToWide(font_name);
  if (FAILED(font_collection->FindFamilyName(font_name_wide.c_str(), &index,
                                             &exists)) ||
      !exists) {
    return std::nullopt;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFamily> font_family;
  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
  if (FAILED(font_collection->GetFontFamily(index, &font_family)) ||
      FAILED(font_family->GetFamilyNames(&family_names))) {
    return std::nullopt;
  }

  return RetrieveLocalizedString(family_names.Get(), locale);
}

}  // namespace win
}  // namespace gfx
