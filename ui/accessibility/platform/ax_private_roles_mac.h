// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_

#include <Availability.h>
#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

// Private WebKit accessibility roles.
#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityWebAreaRole @"AXWebArea"
#endif

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
