// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_PERMISSIONS_UTILS_H_
#define UI_BASE_COCOA_PERMISSIONS_UTILS_H_

#include "base/component_export.h"

namespace ui {

// The ability to screen capture is restricted and requires a permission
// authorization. This function returns `true` if screen capture permission was
// already granted by the user and `false` if it was not.
COMPONENT_EXPORT(UI_BASE) bool IsScreenCaptureAllowed();

// Explicitly request from the user permission to capture the screen. Returns
// `true` if the user granted permission and `false` if the user did not. If the
// user previously declined to give permission, this will just return `false`
// immediately without prompting the user.
COMPONENT_EXPORT(UI_BASE) bool TryPromptUserForScreenCapture();

// Screen sharing will fail if the following conditions are met:
// * The running instance of Chrome has not yet screen shared this session.
// * Chrome has updated.
// * Chrome has fallen out of a macOS system permission cache.
//
// If Chrome has access to capture the screen, take a screenshot of the whole
// screen. This will warm up the cache in the relevant processes. See
// https://crbug.com/1317690 for more information.
COMPONENT_EXPORT(UI_BASE) void WarmScreenCapture();

}  // namespace ui

#endif  // UI_BASE_COCOA_PERMISSIONS_UTILS_H_
