// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_IME_UTIL_CHROMEOS_H_
#define UI_WM_CORE_IME_UTIL_CHROMEOS_H_

#include "ui/aura/window.h"
#include "ui/wm/core/wm_core_export.h"

namespace gfx {
class Rect;
}

namespace wm {

// A property key to store the restore bounds for a window when moved by the
// virtual keyboard.
WM_CORE_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kVirtualKeyboardRestoreBoundsKey;

// Moves |window| to ensure it does not intersect with |rect_in_screen|.
WM_CORE_EXPORT void EnsureWindowNotInRect(aura::Window* window,
                                          const gfx::Rect& rect_in_screen);

// Restores the window bounds when input client loses the focus on the window.
WM_CORE_EXPORT void RestoreWindowBoundsOnClientFocusLost(
    aura::Window* top_level_window);

}  // namespace wm

#endif  // UI_WM_CORE_IME_UTIL_CHROMEOS_H_
