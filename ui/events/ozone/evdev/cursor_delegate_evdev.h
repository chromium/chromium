// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_CURSOR_DELEGATE_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_CURSOR_DELEGATE_EVDEV_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Vector2dF;
class Rect;
}

namespace ui {

class COMPONENT_EXPORT(EVDEV) CursorDelegateEvdev {
 public:
  virtual ~CursorDelegateEvdev() {}

  // Move the cursor from the Evdev thread.
  virtual void MoveCursor(const gfx::Vector2dF& delta) = 0;
  // Move the cursor from the UI or Evdev (e.g. on a tablet).
  virtual void MoveCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) = 0;
  // Move the cursor from the UI or Evdev thread.
  virtual void MoveCursorTo(const gfx::PointF& location) = 0;

  // Location in screen. Either thread, IPC-free.
  virtual gfx::PointF GetLocation() = 0;

  // Cursor visibility. Either thread, IPC-free.
  virtual bool IsCursorVisible() = 0;

  // The bounds that the cursor is confined to. Either thread, IPC-free.
  virtual gfx::Rect GetCursorConfinedBounds() = 0;

  // Any necessary initialization from Evdev thread.
  virtual void InitializeOnEvdev() = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_CURSOR_DELEGATE_EVDEV_H_
