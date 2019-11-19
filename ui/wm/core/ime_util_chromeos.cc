// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/ime_util_chromeos.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace wm {
namespace {

void SetWindowBoundsInScreen(aura::Window* window,
                             const gfx::Rect& bounds_in_screen) {
  window->SetBoundsInScreen(
      bounds_in_screen,
      display::Screen::GetScreen()->GetDisplayNearestView(window));
}

// Moves the window to ensure caret not in rect.
// Returns whether the window was moved or not.
void MoveWindowToEnsureCaretNotInRect(aura::Window* window,
                                      const gfx::Rect& rect_in_screen) {
  gfx::Rect original_window_bounds = window->GetBoundsInScreen();
  if (window->GetProperty(kVirtualKeyboardRestoreBoundsKey)) {
    original_window_bounds =
        *window->GetProperty(kVirtualKeyboardRestoreBoundsKey);
  }

  // Calculate vertical window shift.
  const int top_y =
      std::max(rect_in_screen.y() - original_window_bounds.height(),
               display::Screen::GetScreen()
                   ->GetDisplayNearestView(window)
                   .work_area()
                   .y());

  // No need to move the window up.
  if (top_y >= original_window_bounds.y())
    return;

  // Set restore bounds and move the window.
  window->SetProperty(kVirtualKeyboardRestoreBoundsKey,
                      new gfx::Rect(original_window_bounds));

  gfx::Rect new_bounds_in_screen = original_window_bounds;
  new_bounds_in_screen.set_y(top_y);
  SetWindowBoundsInScreen(window, new_bounds_in_screen);
}

}  // namespace

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kVirtualKeyboardRestoreBoundsKey,
                                   nullptr)

void RestoreWindowBoundsOnClientFocusLost(aura::Window* window) {
  window->GetRootWindow()->ClearProperty(
      aura::client::kEmbeddedWindowEnsureNotInRect);

  // Get restore bounds of the window
  gfx::Rect* vk_restore_bounds =
      window->GetProperty(kVirtualKeyboardRestoreBoundsKey);
  if (!vk_restore_bounds)
    return;

  // Restore the window bounds
  // TODO(yhanada): Don't move the window if a user has moved it while the
  // keyboard is shown.
  if (window->GetBoundsInScreen() != *vk_restore_bounds) {
    SetWindowBoundsInScreen(window, *vk_restore_bounds);
  }
  window->ClearProperty(wm::kVirtualKeyboardRestoreBoundsKey);
}

void EnsureWindowNotInRect(aura::Window* window,
                           const gfx::Rect& rect_in_screen) {
  gfx::Rect original_window_bounds = window->GetBoundsInScreen();
  if (window->GetProperty(wm::kVirtualKeyboardRestoreBoundsKey)) {
    original_window_bounds =
        *window->GetProperty(wm::kVirtualKeyboardRestoreBoundsKey);
  }

  gfx::Rect hidden_window_bounds_in_screen =
      gfx::IntersectRects(rect_in_screen, original_window_bounds);
  if (hidden_window_bounds_in_screen.IsEmpty()) {
    // The window isn't covered by the keyboard, restore the window position if
    // necessary.
    RestoreWindowBoundsOnClientFocusLost(window);
    return;
  }

  MoveWindowToEnsureCaretNotInRect(window, rect_in_screen);
}

EnsureWindowNotInRectHelper::EnsureWindowNotInRectHelper(
    aura::Window* embedding_root)
    : embedding_root_(embedding_root) {
  embedding_root_->AddObserver(this);
}

EnsureWindowNotInRectHelper::~EnsureWindowNotInRectHelper() {
  if (embedding_root_)
    embedding_root_->RemoveObserver(this);
}

void EnsureWindowNotInRectHelper::OnWindowPropertyChanged(aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
  DCHECK_EQ(embedding_root_, window);

  if (key != aura::client::kEmbeddedWindowEnsureNotInRect)
    return;

  aura::Window* top_level = embedding_root_->GetToplevelWindow();
  gfx::Rect* rect_in_screen = embedding_root_->GetProperty(
      aura::client::kEmbeddedWindowEnsureNotInRect);
  if (rect_in_screen)
    EnsureWindowNotInRect(top_level, *rect_in_screen);
  else
    RestoreWindowBoundsOnClientFocusLost(top_level);
}

void EnsureWindowNotInRectHelper::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(embedding_root_, window);
  embedding_root_ = nullptr;
}

}  // namespace wm
