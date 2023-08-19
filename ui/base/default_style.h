// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEFAULT_STYLE_H_
#define UI_BASE_DEFAULT_STYLE_H_

#include "build/build_config.h"
#include "ui/gfx/platform_font.h"

// This file contains the constants that provide the default style for UI
// controls and dialogs.

namespace ui {

// Default font size delta for messages in dialogs. Note that on Windows, the
// "base" font size is determined by consulting the system for the font used in
// native MessageBox dialogs. On Mac, it is [NSFont systemFontSize]. Linux
// consults the default font description for a GTK Widget context. On ChromeOS,
// ui::ResourceBundle provides a description via IDS_UI_FONT_FAMILY_CROS.
constexpr int kMessageFontSizeDelta = 0;

// Default font size delta for views::Badge.
constexpr int kBadgeFontSizeDelta = gfx::PlatformFont::GetFontSizeDelta(9);

// Default font size delta for dialog buttons, textfields, and labels.
// For CR2023, prefer using views::style::STYLE_BODY_3 instead.
constexpr int kLabelFontSizeDelta = gfx::PlatformFont::GetFontSizeDelta(12);

// Font size delta for dialog titles.
#if BUILDFLAG(IS_APPLE)
constexpr int kTitleFontSizeDelta = gfx::PlatformFont::GetFontSizeDelta(14);
#else
constexpr int kTitleFontSizeDelta = gfx::PlatformFont::GetFontSizeDelta(15);
#endif

}  // namespace ui

#endif  // UI_BASE_DEFAULT_STYLE_H_
