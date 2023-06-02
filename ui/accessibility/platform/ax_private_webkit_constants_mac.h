// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_

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

// Native mac notifications fired.
NSString* const NSAccessibilityAutocorrectionOccurredNotification =
    @"AXAutocorrectionOccurred";
NSString* const NSAccessibilityLoadCompleteNotification = @"AXLoadComplete";
NSString* const NSAccessibilityInvalidStatusChangedNotification =
    @"AXInvalidStatusChanged";
NSString* const NSAccessibilityLiveRegionCreatedNotification =
    @"AXLiveRegionCreated";
NSString* const NSAccessibilityLiveRegionChangedNotification =
    @"AXLiveRegionChanged";
NSString* const NSAccessibilityExpandedChanged = @"AXExpandedChanged";
NSString* const NSAccessibilityMenuItemSelectedNotification =
    @"AXMenuItemSelected";
NSString* const NSAccessibilityElementBusyChangedNotification =
    @"AXElementBusyChanged";

// The following native mac notifications are not fired:
// AXLayoutComplete: Voiceover does not use this, it is considered too spammy.

// Attributes used for NSAccessibilitySelectedTextChangedNotification and
// NSAccessibilityValueChangedNotification.
NSString* const NSAccessibilityTextStateChangeTypeKey =
    @"AXTextStateChangeType";
NSString* const NSAccessibilityTextStateSyncKey = @"AXTextStateSync";
NSString* const NSAccessibilityTextSelectionDirection =
    @"AXTextSelectionDirection";
NSString* const NSAccessibilityTextSelectionGranularity =
    @"AXTextSelectionGranularity";
NSString* const NSAccessibilityTextSelectionChangedFocus =
    @"AXTextSelectionChangedFocus";
NSString* const NSAccessibilityTextChangeElement = @"AXTextChangeElement";
NSString* const NSAccessibilityTextEditType = @"AXTextEditType";
NSString* const NSAccessibilityTextChangeValue = @"AXTextChangeValue";
NSString* const NSAccessibilityChangeValueStartMarker =
    @"AXTextChangeValueStartMarker";
NSString* const NSAccessibilityTextChangeValueLength =
    @"AXTextChangeValueLength";
NSString* const NSAccessibilityTextChangeValues = @"AXTextChangeValues";

COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextStateChangeType);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionDirection);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionGranularity);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextEditType);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
