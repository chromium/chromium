// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_CHROMEOS_CURSOR_CONTROLLER_H_
#define UI_EVENTS_OZONE_CHROMEOS_CURSOR_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "ui/display/display.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Manager for per-window cursor settings.
//
// This is used to apply a rotation & acceleration to each vector added to the
// cursor position on ChromeOS.
//
// This has 3 uses:
//
//  (1) Fixing cursor movement direction for rotated displays.
//  (2) Fixing cursor movement speed based on scale factor.
//  (3) Tweaking cursor movement speed on external displays.
//
// This HACK is necessary because ash handles rotation and handles scaling but
// does NOT handle the cursor movement (except that it sends a message to x11 or
// ozone to activate this hack).
//
// TODO(spang): Don't worry, we have a plan to remove this.
class COMPONENT_EXPORT(EVENTS_OZONE) CursorController {
 public:
  class CursorObserver {
   public:
    // Called when cursor location changed.
    virtual void OnCursorLocationChanged(const gfx::PointF& location) = 0;

   protected:
    virtual ~CursorObserver() {}
  };

  static CursorController* GetInstance();

  CursorController(const CursorController&) = delete;
  CursorController& operator=(const CursorController&) = delete;

  void AddCursorObserver(CursorObserver* observer);
  void RemoveCursorObserver(CursorObserver* observer);

  // Changes the rotation & scale applied for a window.
  void SetCursorConfigForWindow(gfx::AcceleratedWidget widget,
                                display::Display::Rotation rotation,
                                float scale);

  // Cleans up all state associated with a window.
  void ClearCursorConfigForWindow(gfx::AcceleratedWidget widget);

  // Applies the current settings for a window to a cursor movement vector.
  //
  // The rotation applies counter-clockwise (to negate clockwise display
  // rotation) and the result is multiplied by scale.
  //
  // e.g. if (dx, dy) = (2, 3) and (scale, rotation) = (2.f, 90deg)
  //      then we set (dx, dy) = (-6, 4)
  //
  // Since scale generally includes DSF, you can think of the input
  // vector unit as DIP and the output vector unit as pixels.
  void ApplyCursorConfigForWindow(gfx::AcceleratedWidget widget,
                                  gfx::Vector2dF* delta) const;

  // Notifies controller of new cursor location.
  void SetCursorLocation(const gfx::PointF& location);

 private:
  CursorController();
  ~CursorController();
  friend struct base::DefaultSingletonTraits<CursorController>;

  struct PerWindowCursorConfiguration {
    display::Display::Rotation rotation;
    float scale;
  };

  typedef std::map<gfx::AcceleratedWidget, PerWindowCursorConfiguration>
      WindowToCursorConfigurationMap;

  mutable base::Lock window_to_cursor_configuration_map_lock_;
  WindowToCursorConfigurationMap window_to_cursor_configuration_map_
      GUARDED_BY(window_to_cursor_configuration_map_lock_);

  mutable base::Lock cursor_observers_lock_;
  std::vector<CursorObserver*> cursor_observers_
      GUARDED_BY(cursor_observers_lock_);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_CHROMEOS_CURSOR_CONTROLLER_H_
