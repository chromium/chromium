// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_CAPTION_BUTTON_LAYOUT_CONSTANTS_H_
#define UI_VIEWS_WINDOW_CAPTION_BUTTON_LAYOUT_CONSTANTS_H_

#include "ui/views/views_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace views {

enum class CaptionButtonLayoutSize {
  // Size of a caption button in a maximized browser window.
  kBrowserCaptionMaximized,

  // Size of a caption button in a restored browser window.
  kBrowserCaptionRestored,

  // Size of a caption button in a non-browser window.
  kNonBrowserCaption,
};

// Default radius of caption button ink drop highlight and mask.
constexpr int kCaptionButtonInkDropDefaultCornerRadius = 14;

// Returns default caption button width.
VIEWS_EXPORT int GetCaptionButtonWidth();

// Calculates the preferred size of an MD-style frame caption button.  Only used
// on ChromeOS and desktop Linux.
VIEWS_EXPORT gfx::Size GetCaptionButtonLayoutSize(CaptionButtonLayoutSize size);

}  // namespace views

#endif  // UI_VIEWS_WINDOW_CAPTION_BUTTON_LAYOUT_CONSTANTS_H_
