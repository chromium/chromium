// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_TOPMOST_WINDOW_FINDER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_TOPMOST_WINDOW_FINDER_H_

#include <set>

#include "base/macros.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {

// Utility class for finding the topmost window at a given screen position.
class VIEWS_EXPORT X11TopmostWindowFinder
    : public ui::EnumerateWindowsDelegate {
 public:
  X11TopmostWindowFinder();
  ~X11TopmostWindowFinder() override;

  // Returns the topmost window at |screen_loc_in_pixels|, ignoring the windows
  // in |ignore|. Returns NULL if the topmost window at |screen_loc_in_pixels|
  // does not belong to Chrome.
  aura::Window* FindLocalProcessWindowAt(const gfx::Point& screen_loc_in_pixels,
                                         const std::set<aura::Window*>& ignore);

  // Returns the topmost window at |screen_loc_in_pixels|.
  XID FindWindowAt(const gfx::Point& screen_loc_in_pixels);

 private:
  // ui::EnumerateWindowsDelegate:
  bool ShouldStopIterating(XID xid) override;

  // Returns true if |window| does not not belong to |ignore|, is visible and
  // contains |screen_loc_|.
  bool ShouldStopIteratingAtLocalProcessWindow(aura::Window* window);

  gfx::Point screen_loc_in_pixels_;
  std::set<aura::Window*> ignore_;
  XID toplevel_ = x11::None;

  DISALLOW_COPY_AND_ASSIGN(X11TopmostWindowFinder);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_TOPMOST_WINDOW_FINDER_H_
