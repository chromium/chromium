// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_

#include <memory>

#include "base/strings/string16.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class PlatformImeController;

// Platform window.
//
// Each instance of PlatformWindow represents a single window in the
// underlying platform windowing system (i.e. X11/Win/OSX).
class PlatformWindow : public PropertyHandler {
 public:
  virtual ~PlatformWindow() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;

  // Informs the window it is going to be destroyed sometime soon. This is only
  // called for specific code paths, for example by Ash, so it shouldn't be
  // assumed this will get called before destruction.
  virtual void PrepareForShutdown() = 0;

  // Sets and gets the bounds of the platform-window. Note that the bounds is in
  // physical pixel coordinates.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetBounds() = 0;

  virtual void SetTitle(const base::string16& title) = 0;

  virtual void SetCapture() = 0;
  virtual void ReleaseCapture() = 0;
  virtual bool HasCapture() const = 0;

  virtual void ToggleFullscreen() = 0;
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;
  virtual PlatformWindowState GetPlatformWindowState() const = 0;

  virtual void SetCursor(PlatformCursor cursor) = 0;

  // Moves the cursor to |location|. Location is in platform window coordinates.
  virtual void MoveCursorTo(const gfx::Point& location) = 0;

  // Confines the cursor to |bounds| when it is in the platform window. |bounds|
  // is in platform window coordinates.
  virtual void ConfineCursorToBounds(const gfx::Rect& bounds) = 0;

  // The PlatformImeController is owned by the PlatformWindow, the ownership is
  // not transferred.
  virtual PlatformImeController* GetPlatformImeController() = 0;

  // Sets and gets the restored bounds of the platform-window.
  virtual void SetRestoredBoundsInPixels(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetRestoredBoundsInPixels() const = 0;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_H_
