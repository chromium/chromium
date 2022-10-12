// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/accessibility/ax_export.h"

// Private WebKit accessibility attributes.
AX_EXPORT constexpr NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRange";
AX_EXPORT constexpr NSString* const NSAccessibilityAccessKeyAttribute =
    @"AXAccessKey";
AX_EXPORT constexpr NSString* const NSAccessibilityARIAAtomicAttribute =
    @"AXARIAAtomic";
AX_EXPORT constexpr NSString* const NSAccessibilityARIABusyAttribute =
    @"AXARIABusy";
AX_EXPORT constexpr NSString* const NSAccessibilityARIAColumnCountAttribute =
    @"AXARIAColumnCount";
AX_EXPORT constexpr NSString* const NSAccessibilityARIAColumnIndexAttribute =
    @"AXARIAColumnIndex";
AX_EXPORT constexpr NSString* const NSAccessibilityARIACurrentAttribute =
    @"AXARIACurrent";
AX_EXPORT constexpr NSString* const NSAccessibilityARIALiveAttribute =
    @"AXARIALive";
AX_EXPORT constexpr NSString* const NSAccessibilityARIAPosInSetAttribute =
    @"AXARIAPosInSet";
AX_EXPORT constexpr NSString* const NSAccessibilityARIARelevantAttribute =
    @"AXARIARelevant";
AX_EXPORT constexpr NSString* const NSAccessibilityARIARowCountAttribute =
    @"AXARIARowCount";
AX_EXPORT constexpr NSString* const NSAccessibilityARIARowIndexAttribute =
    @"AXARIARowIndex";
AX_EXPORT constexpr NSString* const NSAccessibilityARIASetSizeAttribute =
    @"AXARIASetSize";
AX_EXPORT constexpr NSString* const NSAccessibilityAutocompleteValueAttribute =
    @"AXAutocompleteValue";
AX_EXPORT constexpr NSString* const NSAccessibilityBrailleLabelAttribute =
    @"AXBrailleLabel";
AX_EXPORT constexpr NSString* const NSAccessibilityBrailleRoleDescription =
    @"AXBrailleRoleDescription";

AX_EXPORT constexpr NSString* const NSAccessibilityBlockQuoteLevelAttribute =
    @"AXBlockQuoteLevel";
AX_EXPORT constexpr NSString* const NSAccessibilityChromeAXNodeIdAttribute =
    @"ChromeAXNodeId";
AX_EXPORT constexpr NSString* const NSAccessibilityDetailsElementsAttribute =
    @"AXDetailsElements";
AX_EXPORT constexpr NSString* const NSAccessibilityDOMClassList =
    @"AXDOMClassList";
AX_EXPORT constexpr NSString* const NSAccessibilityDOMIdentifierAttribute =
    @"AXDOMIdentifier";
AX_EXPORT constexpr NSString* const NSAccessibilityDropEffectsAttribute =
    @"AXDropEffects";
AX_EXPORT constexpr NSString* const NSAccessibilityEditableAncestorAttribute =
    @"AXEditableAncestor";
AX_EXPORT constexpr NSString* const NSAccessibilityElementBusyAttribute =
    @"AXElementBusy";
AX_EXPORT constexpr NSString* const NSAccessibilityFocusableAncestorAttribute =
    @"AXFocusableAncestor";
AX_EXPORT constexpr NSString* const NSAccessibilityGrabbedAttribute =
    @"AXGrabbed";
AX_EXPORT constexpr NSString* const NSAccessibilityHasPopupAttribute =
    @"AXHasPopup";
AX_EXPORT constexpr NSString* const
    NSAccessibilityHighestEditableAncestorAttribute =
        @"AXHighestEditableAncestor";
AX_EXPORT constexpr NSString* const NSAccessibilityInvalidAttribute =
    @"AXInvalid";
AX_EXPORT constexpr NSString* const NSAccessibilityIsMultiSelectable =
    @"AXIsMultiSelectable";
AX_EXPORT constexpr NSString* const NSAccessibilityKeyShortcutsValueAttribute =
    @"AXKeyShortcutsValue";
AX_EXPORT constexpr NSString* const NSAccessibilityLoadedAttribute =
    @"AXLoaded";
AX_EXPORT constexpr NSString* const NSAccessibilityLoadingProgressAttribute =
    @"AXLoadingProgress";
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
AX_EXPORT constexpr NSString* const NSAccessibilityOwnsAttribute = @"AXOwns";
AX_EXPORT constexpr NSString* const NSAccessibilityPopupValueAttribute =
    @"AXPopupValue";
AX_EXPORT constexpr NSString* const NSAccessibilityVisitedAttribute =
    @"AXVisited";

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
