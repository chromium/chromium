// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_DIRECT_WRITE_H_
#define UI_GFX_WIN_DIRECT_WRITE_H_

#include <dwrite.h>

#include <optional>
#include <string_view>

#include "base/component_export.h"

namespace gfx {
namespace win {

COMPONENT_EXPORT(GFX) void InitializeDirectWrite();

// Creates a DirectWrite factory.
COMPONENT_EXPORT(GFX) void CreateDWriteFactory(IDWriteFactory** factory);

// Returns the global DirectWrite factory.
COMPONENT_EXPORT(GFX) IDWriteFactory* GetDirectWriteFactory();

// Retrieves the localized string for a given locale. If locale is empty,
// retrieves the first element of |names|.
COMPONENT_EXPORT(GFX)
std::optional<std::string> RetrieveLocalizedString(
    IDWriteLocalizedStrings* names,
    const std::string& locale);

// Retrieves the localized font name for a given locale. If locale is empty,
// retrieves the default native font name.
COMPONENT_EXPORT(GFX)
std::optional<std::string> RetrieveLocalizedFontName(std::string_view font_name,
                                                     const std::string& locale);

}  // namespace win
}  // namespace gfx

#endif  // UI_GFX_WIN_DIRECT_WRITE_H_
