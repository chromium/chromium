// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/accessibility/ax_export.h"

// Private WebKit accessibility roles.
AX_EXPORT constexpr NSString* const NSAccessibilityWebAreaRole = @"AXWebArea";

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ROLES_MAC_H_
