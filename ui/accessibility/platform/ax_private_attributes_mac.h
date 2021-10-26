// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/accessibility/ax_export.h"

// Private WebKit accessibility attributes.
AX_EXPORT constexpr NSString* const NSAccessibilityAccessKeyAttribute =
    @"AXAccessKey";
AX_EXPORT constexpr NSString* const NSAccessibilityARIAAtomicAttribute =
    @"AXARIAAtomic";
AX_EXPORT constexpr NSString* const NSAccessibilityARIABusyAttribute =
    @"AXARIABusy";
AX_EXPORT constexpr NSString* const NSAccessibilityARIACurrentAttribute =
    @"AXARIACurrent";
AX_EXPORT constexpr NSString* const NSAccessibilityARIALiveAttribute =
    @"AXARIALive";
AX_EXPORT constexpr NSString* const NSAccessibilityARIARelevantAttribute =
    @"AXARIARelevant";
AX_EXPORT constexpr NSString* const NSAccessibilityAutocompleteValueAttribute =
    @"AXAutocompleteValue";

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
