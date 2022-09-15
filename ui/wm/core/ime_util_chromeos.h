// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_IME_UTIL_CHROMEOS_H_
#define UI_WM_CORE_IME_UTIL_CHROMEOS_H_

#include "base/component_export.h"
#include "ui/aura/window.h"

namespace gfx {
class Rect;
}

namespace wm {

// A property key to store the restore bounds for a window when moved by the
// virtual keyboard.
COMPONENT_EXPORT(UI_WM)
extern const aura::WindowProperty<gfx::Rect*>* const
    kVirtualKeyboardRestoreBoundsKey;

// Moves |window| to ensure it does not intersect with |rect_in_screen|.
COMPONENT_EXPORT(UI_WM)
void EnsureWindowNotInRect(aura::Window* window,
                           const gfx::Rect& rect_in_screen);

// Restores the window bounds when input client loses the focus on the window.
COMPONENT_EXPORT(UI_WM)
void RestoreWindowBoundsOnClientFocusLost(aura::Window* top_level_window);

}  // namespace wm

#endif  // UI_WM_CORE_IME_UTIL_CHROMEOS_H_
