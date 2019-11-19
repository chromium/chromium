// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class X11WindowManager;

// A PlatformScreen implementation for X11.
class X11ScreenOzone : public PlatformScreen,
                       public XEventDispatcher,
                       public XDisplayManager::Delegate {
 public:
  X11ScreenOzone();
  ~X11ScreenOzone() override;

  // Fetch display list through Xlib/XRandR
  void Init();

  // Overridden from ui::PlatformScreen:
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // Overridden from ui::XEventDispatcher:
  bool DispatchXEvent(XEvent* event) override;

 private:
  friend class X11ScreenOzoneTest;

  // Overridden from ui::XDisplayManager::Delegate:
  void OnXDisplayListUpdated() override;
  float GetXDisplayScaleFactor() override;

  gfx::Point GetCursorLocation() const;

  X11WindowManager* const window_manager_;
  std::unique_ptr<ui::XDisplayManager> x11_display_manager_;

  DISALLOW_COPY_AND_ASSIGN(X11ScreenOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
