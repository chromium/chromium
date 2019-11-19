// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DISPLAY_MANAGER_H_
#define UI_BASE_X_X11_DISPLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "ui/display/display.h"
#include "ui/display/display_change_notifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11_types.h"

namespace views {
class DesktopScreenX11Test;
}

namespace ui {
class X11ScreenOzoneTest;

////////////////////////////////////////////////////////////////////////////////
// XDisplayManager class
//
// Responsible for fetching and maintaining list of |display::Display|s
// representing X11 screens connected to the system. XRandR extension is used
// when version >= 1.3 is available, otherwise it falls back to
// |DefaultScreenOfDisplay| Xlib API.
//
// Scale Factor information and simple hooks are delegated to API clients
// through |XDisplayManager::Delegate| interface. To get notifications about
// dynamic display changes, clients must register |DisplayObserver| instances
// and feed |XDisplayManager| with |XEvent|s.
//
// All bounds and size values are assumed to be expressed in pixels.
class COMPONENT_EXPORT(UI_BASE_X) XDisplayManager {
 public:
  class Delegate;

  explicit XDisplayManager(Delegate* delegate);
  virtual ~XDisplayManager();

  void Init();
  bool IsXrandrAvailable() const;
  bool CanProcessEvent(const XEvent& xev);
  bool ProcessEvent(XEvent* xev);
  void UpdateDisplayList();
  void DispatchDelayedDisplayListUpdate();
  display::Display GetPrimaryDisplay() const;

  void AddObserver(display::DisplayObserver* observer);
  void RemoveObserver(display::DisplayObserver* observer);

  const std::vector<display::Display>& displays() const { return displays_; }
  gfx::Point GetCursorLocation() const;

 private:
  friend class ui::X11ScreenOzoneTest;
  friend class views::DesktopScreenX11Test;

  void SetDisplayList(std::vector<display::Display> displays);
  void FetchDisplayList();

  Delegate* const delegate_;
  std::vector<display::Display> displays_;
  display::DisplayChangeNotifier change_notifier_;

  XDisplay* const xdisplay_;
  XID x_root_window_;
  int64_t primary_display_index_ = 0;

  // XRandR version. MAJOR * 100 + MINOR. Zero if no xrandr is present.
  const int xrandr_version_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_ = 0;

  // The task to delay fetching display info. We delay it so that we can
  // coalesce events.
  base::CancelableOnceClosure delayed_update_task_;

  DISALLOW_COPY_AND_ASSIGN(XDisplayManager);
};

class COMPONENT_EXPORT(UI_BASE_X) XDisplayManager::Delegate {
 public:
  virtual ~Delegate() = default;
  virtual void OnXDisplayListUpdated() = 0;
  virtual float GetXDisplayScaleFactor() = 0;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DISPLAY_MANAGER_H_
