// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_

#include <Availability.h>
#import <Foundation/Foundation.h>

#include "base/component_export.h"

// An attribute constant to hold the node ID for round-tripping through the
// accessibility API.

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityChromeAXNodeIdAttribute =
    @"ChromeAXNodeId";

// Accessibility attributes. These are in the macOS 26 SDK as being introduced
// in macOS 26, but they are actually available earlier. Define them in a way
// that will override the SDK definition. Remove when macOS 26 is the minimum
// requirement for Chromium.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityBlockQuoteLevelAttribute @"AXBlockQuoteLevel"
#define NSAccessibilityFontBoldAttribute @"AXFontBold"
#define NSAccessibilityFontItalicAttribute @"AXFontItalic"
#define NSAccessibilityIndexForChildUIElementParameterizedAttribute \
  @"AXIndexForChildUIElement"
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#define NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute \
  @"AXUIElementsForSearchPredicate"
#define NSAccessibilityVisitedAttribute @"AXVisited"
#endif

// Accessibility attributes not found in the SDK.

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityARIABusyAttribute = @"AXARIABusy";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityAutocompleteValueAttribute =
    @"AXAutocompleteValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityDetailsElementsAttribute =
    @"AXDetailsElements";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute =
        @"AXEndTextMarkerForBounds";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityIsMultiSelectable =
    @"AXIsMultiSelectable";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
        @"AXSelectTextWithCriteria";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute =
        @"AXStartTextMarkerForBounds";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute =
        @"AXUIElementCountForSearchPredicate";

// Text markers macOS accessibility attributes.

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRangeWithOptions";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextOperationParameterizedAttribute =
    @"AXTextOperation";

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

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_ATTRIBUTES_MAC_H_
