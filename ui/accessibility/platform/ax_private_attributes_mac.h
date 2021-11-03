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
AX_EXPORT constexpr NSString* const NSAccessibilityDetailsElementsAttribute =
    @"AXDetailsElements";
AX_EXPORT constexpr NSString* const
    NSAccessibilityMathFractionNumeratorAttribute = @"AXMathFractionNumerator";
AX_EXPORT constexpr NSString* const
    NSAccessibilityMathFractionDenominatorAttribute =
        @"AXMathFractionDenominator";
AX_EXPORT constexpr NSString* const NSAccessibilityMathRootRadicandAttribute =
    @"AXMathRootRadicand";
AX_EXPORT constexpr NSString* const NSAccessibilityMathRootIndexAttribute =
    @"AXMathRootIndex";
AX_EXPORT constexpr NSString* const NSAccessibilityMathBaseAttribute =
    @"AXMathBase";
AX_EXPORT constexpr NSString* const NSAccessibilityMathSubscriptAttribute =
    @"AXMathSubscript";
AX_EXPORT constexpr NSString* const NSAccessibilityMathSuperscriptAttribute =
    @"AXMathSuperscript";
AX_EXPORT constexpr NSString* const NSAccessibilityMathUnderAttribute =
    @"AXMathUnder";
AX_EXPORT constexpr NSString* const NSAccessibilityMathOverAttribute =
    @"AXMathOver";
AX_EXPORT constexpr NSString* const NSAccessibilityMathPostscriptsAttribute =
    @"AXMathPostscripts";
AX_EXPORT constexpr NSString* const NSAccessibilityMathPrescriptsAttribute =
    @"AXMathPrescripts";

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
