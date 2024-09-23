// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_DIRECT_WRITE_H_
#define UI_GFX_WIN_DIRECT_WRITE_H_

#include <dwrite.h>

#include <optional>
#include <string_view>

#include "ui/gfx/gfx_export.h"

namespace gfx {
namespace win {

GFX_EXPORT void InitializeDirectWrite();

// Creates a DirectWrite factory.
GFX_EXPORT void CreateDWriteFactory(IDWriteFactory** factory);

// Returns the global DirectWrite factory.
GFX_EXPORT IDWriteFactory* GetDirectWriteFactory();

// Retrieves the localized string for a given locale. If locale is empty,
// retrieves the first element of |names|.
GFX_EXPORT std::optional<std::string> RetrieveLocalizedString(
    IDWriteLocalizedStrings* names,
    const std::string& locale);

// Retrieves the localized font name for a given locale. If locale is empty,
// retrieves the default native font name.
GFX_EXPORT std::optional<std::string> RetrieveLocalizedFontName(
    std::string_view font_name,
    const std::string& locale);

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_WIN_DIRECT_WRITE_H_
