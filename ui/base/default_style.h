// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEFAULT_STYLE_H_
#define UI_BASE_DEFAULT_STYLE_H_

#include "build/build_config.h"

// This file contains the constants that provide the default style for UI
// controls and dialogs.

namespace ui {

// Default font size delta for messages in dialogs. Note that on Windows, the
// "base" font size is determined by consulting the system for the font used in
// native MessageBox dialogs. On Mac, it is [NSFont systemFontSize]. Linux
// consults the default font description for a GTK Widget context. On ChromeOS,
// ui::ResourceBundle provides a description via IDS_UI_FONT_FAMILY_CROS.
const int kMessageFontSizeDelta = 0;

// Default font size delta for dialog buttons, textfields, and labels.
#if BUILDFLAG(IS_APPLE)
// Aim for 12pt for Cocoa labels ([NSFont systemFontSize] is typically 13pt).
const int kLabelFontSizeDelta = -1;
#else
const int kLabelFontSizeDelta = 0;
#endif

// Font size delta for dialog titles.
#if BUILDFLAG(IS_APPLE)
const int kTitleFontSizeDelta = 1;
#else
const int kTitleFontSizeDelta = 3;
#endif

}  // namespace ui

#endif  // UI_BASE_DEFAULT_STYLE_H_
