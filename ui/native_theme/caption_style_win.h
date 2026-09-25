// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_CAPTION_STYLE_WIN_H_
#define UI_NATIVE_THEME_CAPTION_STYLE_WIN_H_

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"

namespace ui {

// Enables caching of the caption style returned by
// CaptionStyle::FromSystemSettings().
//
// Reading the style from the OS is expensive, but it can only be cached if
// something watches the OS settings and drops the cache when they change.
// The caller must retain the returned runner to keep caching enabled.
// Releasing the runner (or letting it go out of scope) disables caching and
// clears the cache.
//
// Only one runner may exist in the process at a time. It is a fatal error
// (CHECK) to call this while another runner is active.
[[nodiscard]] COMPONENT_EXPORT(NATIVE_THEME) base::ScopedClosureRunner
    EnableCaptionStyleCaching();

// Drops the process-wide cache of the caption style returned by
// CaptionStyle::FromSystemSettings(). Must be called whenever the Windows
// closed-captioning settings change, so that the next call re-reads them.
// May be called from any thread.
COMPONENT_EXPORT(NATIVE_THEME)
void InvalidateCaptionStyleCache();

}  // namespace ui

#endif  // UI_NATIVE_THEME_CAPTION_STYLE_WIN_H_
