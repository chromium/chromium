// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_

#include <Availability.h>

// Accessibility roles. These are in the macOS 26 SDK as being introduced in
// macOS 26, but they are actually available earlier. Define them in a way that
// will override the SDK definition. Remove when macOS 26 is the minimum
// requirement for Chromium.
#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityListMarkerRole @"AXListMarker"
#define NSAccessibilityWebAreaRole @"AXWebArea"
#endif

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
