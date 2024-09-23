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

}  // namespace ui

#endif  // UI_BASE_COCOA_CURSOR_UTILS_H_
