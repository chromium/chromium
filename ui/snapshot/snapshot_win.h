// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_WIN_H_
#define UI_SNAPSHOT_SNAPSHOT_WIN_H_

#include <windows.h>

#include "ui/snapshot/snapshot_export.h"

namespace gfx {
class Image;
class Rect;
}

namespace ui {
namespace internal {

// Grabs a snapshot of the desktop. No security checks are done.  This is
// intended to be used for debugging purposes where no BrowserProcess instance
// is available (ie. tests). DO NOT use in a result of user action.
//
// snapshot_bounds_in_pixels is the area relative to clip_rect_in_pixels that
// should be captured.  Areas outside clip_rect_in_pixels are filled white.
// clip_rect_in_pixels is relative to the client area of the window.
SNAPSHOT_EXPORT bool GrabHwndSnapshot(
    HWND window_handle,
    const gfx::Rect& snapshot_bounds_in_pixels,
    const gfx::Rect& clip_rect_in_pixels,
    gfx::Image* image);

}  // namespace internal
}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_WIN_H_
