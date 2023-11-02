// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_COMMON_PLATFORM_WINDOW_DEFAULTS_H_
#define UI_PLATFORM_WINDOW_COMMON_PLATFORM_WINDOW_DEFAULTS_H_

#include "base/component_export.h"

namespace ui {

// Returns true if PlatformWindow should use test configuration. Will return
// false by default, unless test::EnableTestConfigForPlatformWindows() has been
// called, then it will return true.
COMPONENT_EXPORT(PLATFORM_WINDOW_COMMON) bool UseTestConfigForPlatformWindows();

namespace test {

// Sets that PlatformWindow should use test configuration. This can safely be
// called on all platforms but only has an effect for X11 and Wayland.

// For X11 this sets the value of the |override_redirect| attribute used when
// creating an X11 window to true. It is necessary to set this flag on for
// various tests, otherwise the call to Show() blocks because it never receives
// the MapNotify event. It is unclear why this is necessary, but might be
// related to calls to XInitThreads().
//
// For Wayland, forces visual size updates on every configuration event.
COMPONENT_EXPORT(PLATFORM_WINDOW_COMMON)
void EnableTestConfigForPlatformWindows();

}  // namespace test
}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_COMMON_PLATFORM_WINDOW_DEFAULTS_H_
