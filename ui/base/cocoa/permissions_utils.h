// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_PERMISSIONS_UTILS_H_
#define UI_BASE_COCOA_PERMISSIONS_UTILS_H_

#include "base/component_export.h"

namespace ui {

// Heuristic to check screen capture permission.
// Starting on macOS 10.15, the ability to screen capture is restricted and
// requires a permission authorization. There is no direct way to query the
// permission state, so this uses a heuristic to evaluate whether the permission
// has been granted.
COMPONENT_EXPORT(UI_BASE) bool IsScreenCaptureAllowed();

// Heuristic to prompt the user if they have never been prompted for permission.
// Starting on macOS 10.15, not only can we not tell if we have has permission
// granted, we also can't tell if we have requested the permission before. We
// must try capture a stream and the OS will show a modal dialog asking the user
// for permission if Chrome is not in the permission app list, then return
// a stream based on whether permission is granted.
// Returns whether or not permission was granted.
COMPONENT_EXPORT(UI_BASE) bool TryPromptUserForScreenCapture();

}  // namespace ui

#endif  // UI_BASE_COCOA_PERMISSIONS_UTILS_H_
