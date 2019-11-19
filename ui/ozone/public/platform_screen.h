// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_
#define UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_

#include "ui/gfx/native_widget_types.h"

namespace display {
class Display;
class DisplayObserver;
}  // namespace display

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

// PlatformScreen is an abstract base class for an interface to an Ozone
// platform's functionality exposed to Chrome via display::Screen.
//
// Recall that in Chrome, a |Screen| is the union of all attached |Display|
// instances. The |Screen|'s coordinate system is in DIP pixels (so that
// it can reasonably support |Display|s of differing pixel densities.) The
// |Screen|'s origin is the top-left corner of the primary |Display| in the
// |Screen|. Coordinates increase down and to the right.
//
// TODO(rjkroege): Add ascii art?
class PlatformScreen {
 public:
  PlatformScreen() = default;
  virtual ~PlatformScreen() = default;

  // Provide a |display:;Display| for each physical display available to Chrome.
  virtual const std::vector<display::Display>& GetAllDisplays() const = 0;

  // Returns the |Display| whose origin (top left corner) is 0,0 in the
  // |Screen|.
  virtual display::Display GetPrimaryDisplay() const = 0;

  // Returns the Display occupied by |widget|.
  // TODO(rjkroege) This method might be unused?
  // TODO(rjkroege): How should we support unified mode?
  virtual display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const = 0;

  // Returns cursor position in DIPs relative to the |Screen|'s origin.
  // TODO(rjkroege): Verify these semantics.
  virtual gfx::Point GetCursorScreenPoint() const = 0;

  virtual gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const = 0;

  // Returns the |Display| nearest the specified point. |point| must be in DIPs.
  virtual display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const = 0;

  // Returns the |Display| that most closely intersects the provided rect if one
  // exists.
  // TODO(rjk): Update the code to track this.
  virtual display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const = 0;

  // Adds/Removes display observers.
  virtual void AddObserver(display::DisplayObserver* observer) = 0;
  virtual void RemoveObserver(display::DisplayObserver* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformScreen);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_SCREEN_H_