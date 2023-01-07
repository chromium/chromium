// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_EVENT_CREATION_UTILS_H_
#define UI_BASE_WIN_EVENT_CREATION_UTILS_H_

#include "base/component_export.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

// Send a mouse event to Windows input queue using ::SendInput, to screen
// point |point|. Returns true if the mouse event was sent, false if not.
COMPONENT_EXPORT(UI_BASE)
bool SendMouseEvent(const gfx::Point& point, int flags);

}  // namespace ui

#endif  // UI_BASE_WIN_EVENT_CREATION_UTILS_H_
