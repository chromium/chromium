// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CURSOR_UTILS_H_
#define UI_BASE_COCOA_CURSOR_UTILS_H_

#include "base/component_export.h"

#ifdef __OBJC__
@class NSCursor;
#endif  // __OBJC__

namespace ui {

class Cursor;

#ifdef __OBJC__
COMPONENT_EXPORT(UI_BASE)
NSCursor* GetNativeCursor(const ui::Cursor& cursor);
#endif  // __OBJC__

// Returns macOS's accessibility pointer size user preference. The OS renders
// larger Chrome and web content cursors using this scale factor (1.0 - 4.0).
// Note: Renderers and other sandboxed processes get stale NSDefault values.
// This returns a cached value unless `force_update` is true.
COMPONENT_EXPORT(UI_BASE)
float GetCursorAccessibilityScaleFactor(bool force_update = false);

}  // namespace ui

#endif  // UI_BASE_COCOA_CURSOR_UTILS_H_
