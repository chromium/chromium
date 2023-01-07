// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CURSOR_UTILS_H_
#define UI_BASE_COCOA_CURSOR_UTILS_H_

#include "base/component_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

COMPONENT_EXPORT(UI_BASE)
gfx::NativeCursor GetNativeCursor(const ui::Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_COCOA_CURSOR_UTILS_H_
