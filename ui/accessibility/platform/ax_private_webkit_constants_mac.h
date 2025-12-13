// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_

#include <Availability.h>
#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

namespace ui {

enum AXTextStateChangeType {
  AXTextStateChangeTypeUnknown,
  AXTextStateChangeTypeEdit,
  AXTextStateChangeTypeSelectionMove,
  AXTextStateChangeTypeSelectionExtend
};

enum AXTextSelectionDirection {
  AXTextSelectionDirectionUnknown,
  AXTextSelectionDirectionBeginning,
  AXTextSelectionDirectionEnd,
  AXTextSelectionDirectionPrevious,
  AXTextSelectionDirectionNext,
  AXTextSelectionDirectionDiscontiguous
};

enum AXTextSelectionGranularity {
  AXTextSelectionGranularityUnknown,
  AXTextSelectionGranularityCharacter,
  AXTextSelectionGranularityWord,
  AXTextSelectionGranularityLine,
  AXTextSelectionGranularitySentence,
  AXTextSelectionGranularityParagraph,
  AXTextSelectionGranularityPage,
  AXTextSelectionGranularityDocument,
  AXTextSelectionGranularityAll
};

enum AXTextEditType {
  AXTextEditTypeUnknown,
  AXTextEditTypeDelete,
  AXTextEditTypeInsert,
  AXTextEditTypeTyping,
  AXTextEditTypeDictation,
  AXTextEditTypeCut,
  AXTextEditTypePaste,
  AXTextEditTypeAttributesChange
};

// Native macOS notifications fired.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityAutocorrectionOccurredNotification \
  @"AXAutocorrectionOccurred"
#endif

COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityElementBusyChangedNotification =
    @"AXElementBusyChanged";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityExpandedChanged = @"AXExpandedChanged";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityInvalidStatusChangedNotification =
    @"AXInvalidStatusChanged";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLiveRegionChangedNotification =
    @"AXLiveRegionChanged";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLiveRegionCreatedNotification =
    @"AXLiveRegionCreated";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityLoadCompleteNotification =
    @"AXLoadComplete";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityMenuItemSelectedNotification =
    @"AXMenuItemSelected";

// The following native macOS notifications are not fired:
// AXLayoutComplete: Voiceover does not use this, it is considered too spammy.

// Attributes used for NSAccessibilitySelectedTextChangedNotification and
// NSAccessibilityValueChangedNotification.
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityChangeValueStartMarker =
    @"AXTextChangeValueStartMarker";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextChangeElement =
    @"AXTextChangeElement";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextChangeValue = @"AXTextChangeValue";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextChangeValueLength =
    @"AXTextChangeValueLength";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextChangeValues =
    @"AXTextChangeValues";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextEditType = @"AXTextEditType";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextSelectionChangedFocus =
    @"AXTextSelectionChangedFocus";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextSelectionDirection =
    @"AXTextSelectionDirection";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextSelectionGranularity =
    @"AXTextSelectionGranularity";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextStateChangeTypeKey =
    @"AXTextStateChangeType";
COMPONENT_EXPORT(AX_PLATFORM)
constexpr NSString* const NSAccessibilityTextStateSyncKey = @"AXTextStateSync";

// Actions.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"
#endif

COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextStateChangeType);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionDirection);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionGranularity);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextEditType);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
