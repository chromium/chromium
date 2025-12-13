// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_

#include <Availability.h>
#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

// General macOS accessibility attributes.

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
constexpr NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRange";
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
constexpr NSString* const NSAccessibilityEndTextMarkerAttribute =
    @"AXEndTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute =
        @"AXEndTextMarkerForBounds";
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
constexpr NSString* const
    NSAccessibilityIndexForChildUIElementParameterizedAttribute =
        @"AXIndexForChildUIElement";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityIndexForTextMarkerParameterizedAttribute =
        @"AXIndexForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityInvalidAttribute = @"AXInvalid";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityIsMultiSelectable =
    @"AXIsMultiSelectable";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityKeyShortcutsValueAttribute =
    @"AXKeyShortcutsValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLineTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLoadedAttribute = @"AXLoaded";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLoadingProgressAttribute =
    @"AXLoadingProgress";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathBaseAttribute = @"AXMathBase";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathFractionDenominatorAttribute =
    @"AXMathFractionDenominator";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathFractionNumeratorAttribute =
    @"AXMathFractionNumerator";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathOverAttribute = @"AXMathOver";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathPostscriptsAttribute =
    @"AXMathPostscripts";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathPrescriptsAttribute =
    @"AXMathPrescripts";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathRootIndexAttribute =
    @"AXMathRootIndex";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathRootRadicandAttribute =
    @"AXMathRootRadicand";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathSubscriptAttribute =
    @"AXMathSubscript";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathSuperscriptAttribute =
    @"AXMathSuperscript";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMathUnderAttribute = @"AXMathUnder";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityOwnsAttribute = @"AXOwns";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityPopupValueAttribute = @"AXPopupValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
        @"AXSelectTextWithCriteria";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityStartTextMarkerAttribute =
    @"AXStartTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute =
        @"AXStartTextMarkerForBounds";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute =
        @"AXUIElementCountForSearchPredicate";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityValueAutofillAvailableAttribute =
    @"AXValueAutofillAvailable";

// Text markers macOS accessibility attributes.

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRangeWithOptions";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute =
        @"AXBoundsForTextMarkerRange";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftLineTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftWordTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute =
        @"AXLengthForTextMarkerRange";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityLineForTextMarkerParameterizedAttribute =
        @"AXLineForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextLineEndTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextParagraphEndTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextSentenceEndTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextWordEndTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXParagraphTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousLineStartTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousParagraphStartTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousSentenceStartTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousWordStartTextMarkerForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightLineTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightWordTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXSentenceTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityStringForTextMarkerRangeParameterizedAttribute =
        @"AXStringForTextMarkerRange";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXStyleTextMarkerRangeForTextMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerForIndexParameterizedAttribute =
        @"AXTextMarkerForIndex";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerForPositionParameterizedAttribute =
        @"AXTextMarkerForPosition";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerIsValidParameterizedAttribute =
        @"AXTextMarkerIsValid";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerRangeForLineParameterizedAttribute =
        @"AXTextMarkerRangeForLine";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute =
        @"AXTextMarkerRangeForUIElement";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute =
        @"AXTextMarkerRangeForUnorderedTextMarkers";
// COMPONENT_EXPORT(AX_PLATFORM) constexpr NSString* const
// NSAccessibilityTextOperationMarkerRanges = @"AXTextOperationMarkerRanges";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityUIElementForTextMarkerParameterizedAttribute =
        @"AXUIElementForTextMarker";

// Debug macOS accessibility attributes.

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerDebugDescription";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerNodeDebugDescription";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerRangeDebugDescription";

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityBlockQuoteLevelAttribute @"AXBlockQuoteLevel"
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#define NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute \
  @"AXUIElementsForSearchPredicate"
#define NSAccessibilityVisitedAttribute @"AXVisited"
#endif

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
