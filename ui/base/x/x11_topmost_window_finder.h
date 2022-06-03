// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_TOPMOST_WINDOW_FINDER_H_
#define UI_BASE_X_X11_TOPMOST_WINDOW_FINDER_H_

#include "base/component_export.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// Utility class for finding the topmost window at a given screen position.
class COMPONENT_EXPORT(UI_BASE_X) XTopmostWindowFinder {
 public:
  XTopmostWindowFinder();
  virtual ~XTopmostWindowFinder();

  // Returns the topmost window at |screen_loc_px|.
  virtual x11::Window FindWindowAt(const gfx::Point& screen_loc_px) = 0;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_TOPMOST_WINDOW_FINDER_H_
