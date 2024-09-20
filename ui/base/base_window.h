// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_BASE_WINDOW_H_
#define UI_BASE_BASE_WINDOW_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace ui {

// Provides an interface to perform actions on windows, and query window
// state.
class COMPONENT_EXPORT(UI_BASE) BaseWindow {
 public:
  // Returns true if the window is currently the active/focused window.
  virtual bool IsActive() const = 0;

  // Returns true if the window is maximized (aka zoomed).
  virtual bool IsMaximized() const = 0;

  // Returns true if the window is minimized.
  virtual bool IsMinimized() const = 0;

  // Returns true if the window is full screen.
  virtual bool IsFullscreen() const = 0;

  // Returns true if the window is fully restored (not Fullscreen, Maximized,
  // Minimized).
  static bool IsRestored(const BaseWindow& window);

  // Return a platform dependent identifier for this window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the nonmaximized bounds of the window (even if the window is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetRestoredBounds() const = 0;

  // Returns the restore state for the window (platform dependent).
  virtual ui::mojom::WindowShowState GetRestoredState() const = 0;

  // Retrieves the window's current bounds, including its window.
  // This will only differ from GetRestoredBounds() for maximized
  // and minimized windows.
  virtual gfx::Rect GetBounds() const = 0;

  // Shows the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Hides the window.
  virtual void Hide() = 0;

  // Returns whether the window is visible.
  virtual bool IsVisible() const = 0;

  // Show the window, but do not activate it. Does nothing if window
  // is already visible.
  virtual void ShowInactive() = 0;

  // Closes the window as soon as possible. The close action may be delayed
  // if an operation is in progress (e.g. a drag operation).
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Deactivates the window, making the next window in the Z order the active
  // window.
  virtual void Deactivate() = 0;

  // Maximizes/minimizes/restores the window.
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Flashes the taskbar item associated with this window.
  // Set |flash| to true to initiate flashing, false to stop flashing.
  virtual void FlashFrame(bool flash) = 0;

  // Returns the z-order level of the window.
  virtual ZOrderLevel GetZOrderLevel() const = 0;

  // Sets the z-order level of the window.
  virtual void SetZOrderLevel(ZOrderLevel order) = 0;
};

}  // namespace ui

#endif  // UI_BASE_BASE_WINDOW_H_
