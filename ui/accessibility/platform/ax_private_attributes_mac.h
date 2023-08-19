// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

// Private WebKit accessibility attributes.
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRange";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityAccessKeyAttribute = @"AXAccessKey";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIAAtomicAttribute = @"AXARIAAtomic";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIABusyAttribute = @"AXARIABusy";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIAColumnCountAttribute =
    @"AXARIAColumnCount";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIAColumnIndexAttribute =
    @"AXARIAColumnIndex";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIACurrentAttribute =
    @"AXARIACurrent";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIALiveAttribute = @"AXARIALive";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIAPosInSetAttribute =
    @"AXARIAPosInSet";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIARelevantAttribute =
    @"AXARIARelevant";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIARowCountAttribute =
    @"AXARIARowCount";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIARowIndexAttribute =
    @"AXARIARowIndex";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIASetSizeAttribute =
    @"AXARIASetSize";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityAutocompleteValueAttribute =
    @"AXAutocompleteValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityBrailleLabelAttribute =
    @"AXBrailleLabel";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityBrailleRoleDescription =
    @"AXBrailleRoleDescription";

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityBlockQuoteLevelAttribute =
    @"AXBlockQuoteLevel";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityChromeAXNodeIdAttribute =
    @"ChromeAXNodeId";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityDetailsElementsAttribute =
    @"AXDetailsElements";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityDOMClassList = @"AXDOMClassList";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityDOMIdentifierAttribute =
    @"AXDOMIdentifier";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityDropEffectsAttribute =
    @"AXDropEffects";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityEditableAncestorAttribute =
    @"AXEditableAncestor";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityElementBusyAttribute =
    @"AXElementBusy";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityErrorMessageElementsAttribute =
    @"AXErrorMessageElements";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityFocusableAncestorAttribute =
    @"AXFocusableAncestor";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityGrabbedAttribute = @"AXGrabbed";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityHasPopupAttribute = @"AXHasPopup";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityHighestEditableAncestorAttribute =
    @"AXHighestEditableAncestor";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityInvalidAttribute = @"AXInvalid";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityIsMultiSelectable =
    @"AXIsMultiSelectable";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityKeyShortcutsValueAttribute =
    @"AXKeyShortcutsValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLoadedAttribute = @"AXLoaded";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLoadingProgressAttribute =
    @"AXLoadingProgress";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathFractionNumeratorAttribute =
    @"AXMathFractionNumerator";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathFractionDenominatorAttribute =
    @"AXMathFractionDenominator";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathRootRadicandAttribute =
    @"AXMathRootRadicand";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathRootIndexAttribute =
    @"AXMathRootIndex";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathBaseAttribute = @"AXMathBase";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathSubscriptAttribute =
    @"AXMathSubscript";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathSuperscriptAttribute =
    @"AXMathSuperscript";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathUnderAttribute = @"AXMathUnder";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathOverAttribute = @"AXMathOver";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathPostscriptsAttribute =
    @"AXMathPostscripts";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathPrescriptsAttribute =
    @"AXMathPrescripts";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityOwnsAttribute = @"AXOwns";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityPopupValueAttribute = @"AXPopupValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityVisitedAttribute = @"AXVisited";

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
