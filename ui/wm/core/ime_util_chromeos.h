// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_IME_UTIL_CHROMEOS_H_
#define UI_WM_CORE_IME_UTIL_CHROMEOS_H_

#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/wm_core_export.h"

namespace gfx {
class Rect;
}

namespace wm {

// A property key to store the restore bounds for a window when moved by the
// virtual keyboard.
WM_CORE_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kVirtualKeyboardRestoreBoundsKey;

// Moves |window| to ensure it does not intersect with |rect_in_screen| if it
// does not belong to an embedded window tree. Otherwise, sets |rect_in_screen|
// in kEmbeddedWindowEnsureNotInRect window property on the root window of the
// embedded tree so that the embedding side could forward the call to the
// relevant top level window. See also EnsureWindowNotInRectHelper.
WM_CORE_EXPORT void EnsureWindowNotInRect(aura::Window* window,
                                          const gfx::Rect& rect_in_screen);

// Restores the window bounds when input client loses the focus on the window.
WM_CORE_EXPORT void RestoreWindowBoundsOnClientFocusLost(
    aura::Window* top_level_window);

// Helper to call EnsureWindowNotInRect/RestoreWindowBoundsOnClientFocusLost
// on the top-level window of an embedding root when its
// kEmbeddedWindowEnsureNotInRect window property changes.
class WM_CORE_EXPORT EnsureWindowNotInRectHelper : public aura::WindowObserver {
 public:
  explicit EnsureWindowNotInRectHelper(aura::Window* embedding_root);
  ~EnsureWindowNotInRectHelper() override;

 private:
  // aura::WindowObsever:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroyed(aura::Window* window) override;

  aura::Window* embedding_root_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(EnsureWindowNotInRectHelper);
};

}  // namespace wm

#endif  // UI_WM_CORE_IME_UTIL_CHROMEOS_H_
