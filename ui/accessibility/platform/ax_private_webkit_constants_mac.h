// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_

#include <Availability.h>
#import <Foundation/Foundation.h>

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

// Native macOS notifications fired. These are in the macOS 26 SDK as being
// introduced in macOS 26, but they are actually available earlier. Define them
// in a way that will override the SDK definition. Remove when macOS 26 is the
// minimum requirement for Chromium.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityAutocorrectionOccurredNotification \
  @"AXAutocorrectionOccurred"
#endif

// Attributes. These are in the macOS 26 SDK as being introduced in macOS 26,
// but they are actually available earlier. Define them in a way that will
// override the SDK definition. Remove when macOS 26 is the minimum requirement
// for Chromium.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityTextStateChangeTypeKey @"AXTextStateChangeType"
#define NSAccessibilityTextStateSyncKey @"AXTextStateSync"
#endif

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

// Actions. This is in the macOS 26 SDK as being introduced in macOS 26, but it
// is actually available earlier. Define it in a way that will override the SDK
// definition. Remove when macOS 26 is the minimum requirement for Chromium.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"
#endif

// Search Keys. This is in the macOS 26 SDK as being introduced in macOS 26, but
// it is actually available earlier. Define it in a way that will override the
// SDK definition. Remove when macOS 26 is the minimum requirement for Chromium.

#if !defined(__MAC_26_0) || __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_26_0
#define NSAccessibilityAnyTypeSearchKey @"AXAnyTypeSearchKey"
#define NSAccessibilityBlockquoteSameLevelSearchKey \
  @"AXBlockquoteSameLevelSearchKey"
#define NSAccessibilityBlockquoteSearchKey @"AXBlockquoteSearchKey"
#define NSAccessibilityBoldFontSearchKey @"AXBoldFontSearchKey"
#define NSAccessibilityButtonSearchKey @"AXButtonSearchKey"
#define NSAccessibilityCheckBoxSearchKey @"AXCheckBoxSearchKey"
#define NSAccessibilityControlSearchKey @"AXControlSearchKey"
#define NSAccessibilityDifferentTypeSearchKey @"AXDifferentTypeSearchKey"
#define NSAccessibilityFontChangeSearchKey @"AXFontChangeSearchKey"
#define NSAccessibilityFontColorChangeSearchKey @"AXFontColorChangeSearchKey"
#define NSAccessibilityFrameSearchKey @"AXFrameSearchKey"
#define NSAccessibilityGraphicSearchKey @"AXGraphicSearchKey"
#define NSAccessibilityHeadingLevel1SearchKey @"AXHeadingLevel1SearchKey"
#define NSAccessibilityHeadingLevel2SearchKey @"AXHeadingLevel2SearchKey"
#define NSAccessibilityHeadingLevel3SearchKey @"AXHeadingLevel3SearchKey"
#define NSAccessibilityHeadingLevel4SearchKey @"AXHeadingLevel4SearchKey"
#define NSAccessibilityHeadingLevel5SearchKey @"AXHeadingLevel5SearchKey"
#define NSAccessibilityHeadingLevel6SearchKey @"AXHeadingLevel6SearchKey"
#define NSAccessibilityHeadingSameLevelSearchKey @"AXHeadingSameLevelSearchKey"
#define NSAccessibilityHeadingSearchKey @"AXHeadingSearchKey"
#define NSAccessibilityItalicFontSearchKey @"AXItalicFontSearchKey"
#define NSAccessibilityLandmarkSearchKey @"AXLandmarkSearchKey"
#define NSAccessibilityLinkSearchKey @"AXLinkSearchKey"
#define NSAccessibilityListSearchKey @"AXListSearchKey"
#define NSAccessibilityLiveRegionSearchKey @"AXLiveRegionSearchKey"
#define NSAccessibilityMisspelledWordSearchKey @"AXMisspelledWordSearchKey"
#define NSAccessibilityOutlineSearchKey @"AXOutlineSearchKey"
#define NSAccessibilityPlainTextSearchKey @"AXPlainTextSearchKey"
#define NSAccessibilityRadioGroupSearchKey @"AXRadioGroupSearchKey"
#define NSAccessibilitySameTypeSearchKey @"AXSameTypeSearchKey"
#define NSAccessibilityStaticTextSearchKey @"AXStaticTextSearchKey"
#define NSAccessibilityStyleChangeSearchKey @"AXStyleChangeSearchKey"
#define NSAccessibilityTableSameLevelSearchKey @"AXTableSameLevelSearchKey"
#define NSAccessibilityTableSearchKey @"AXTableSearchKey"
#define NSAccessibilityTextFieldSearchKey @"AXTextFieldSearchKey"
#define NSAccessibilityUnderlineSearchKey @"AXUnderlineSearchKey"
#define NSAccessibilityUnvisitedLinkSearchKey @"AXUnvisitedLinkSearchKey"
#define NSAccessibilityVisitedLinkSearchKey @"AXVisitedLinkSearchKey"
#endif

COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextStateChangeType);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionDirection);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextSelectionGranularity);
COMPONENT_EXPORT(AX_PLATFORM) const char* ToString(AXTextEditType);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PRIVATE_WEBKIT_CONSTANTS_MAC_H_
