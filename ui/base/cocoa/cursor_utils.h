// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CURSOR_UTILS_H_
#define UI_BASE_COCOA_CURSOR_UTILS_H_

#import <AppKit/AppKit.h>

#include "base/component_export.h"

namespace ui {

class Cursor;

COMPONENT_EXPORT(UI_BASE)
NSCursor* GetNativeCursor(const ui::Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_COCOA_CURSOR_UTILS_H_
