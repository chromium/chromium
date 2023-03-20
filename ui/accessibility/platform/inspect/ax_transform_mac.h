// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TRANSFORM_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TRANSFORM_MAC_H_

#import <Accessibility/Accessibility.h>
#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "base/values.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer_mac.h"

namespace ui {

// Returns the base::Value representation of the given NSObject.
COMPONENT_EXPORT(AX_PLATFORM)
base::Value AXNSObjectToBaseValue(id, const AXTreeIndexerMac*);

// Returns the base::Value::Dict representation of the given AXCustomContent.
base::Value::Dict AXCustomContentToBaseValue(AXCustomContent*)
    API_AVAILABLE(macosx(11.0));

// Returns the base::Value representation of the given NSAccessibilityElement.
COMPONENT_EXPORT(AX_PLATFORM)
base::Value AXElementToBaseValue(id, const AXTreeIndexerMac*);

// Returns the base::Value representation of the given AXPosition.
base::Value AXPositionToBaseValue(const AXPlatformNodeDelegate::AXPosition&,
                                  const AXTreeIndexerMac*);

// Returns the base::Value representation of the given AXTextMarker.
base::Value AXTextMarkerToBaseValue(id, const AXTreeIndexerMac*);

// Returns the base::Value representation of the given AXTextMarkerRange.
base::Value AXTextMarkerRangeToBaseValue(id, const AXTreeIndexerMac*);

// Returns the base::Value::Dict representation of the given NSAttributedString.
COMPONENT_EXPORT(AX_PLATFORM)
base::Value NSAttributedStringToBaseValue(NSAttributedString*,
                                          const AXTreeIndexerMac*);

// Returns the base::Value representation of CGColorRef in the form CGColor(r,
// g, b, a).
COMPONENT_EXPORT(AX_PLATFORM)
base::Value CGColorRefToBaseValue(CGColorRef color);

// Returns the base::Value representation of nil.
COMPONENT_EXPORT(AX_PLATFORM) base::Value AXNilToBaseValue();

// Returns the base::Value::List representation of the given NSArray.
COMPONENT_EXPORT(AX_PLATFORM)
base::Value::List AXNSArrayToBaseValue(NSArray*, const AXTreeIndexerMac*);

// Returns the base::Value::Dict representation of the given NSDictionary.
COMPONENT_EXPORT(AX_PLATFORM)
base::Value::Dict AXNSDictionaryToBaseValue(NSDictionary*,
                                            const AXTreeIndexerMac*);

// Returns the base::Value::Dict representation of the given NSPoint.
COMPONENT_EXPORT(AX_PLATFORM) base::Value::Dict AXNSPointToBaseValue(NSPoint);

// Returns the base::Value::Dict representation of the given NSSize.
COMPONENT_EXPORT(AX_PLATFORM) base::Value::Dict AXNSSizeToBaseValue(NSSize);

// Returns the base::Value::Dict representation of the given NSRect.
COMPONENT_EXPORT(AX_PLATFORM) base::Value::Dict AXNSRectToBaseValue(NSRect);

// Returns the base::Value::Dict representation of the given NSRange.
COMPONENT_EXPORT(AX_PLATFORM) base::Value::Dict AXNSRangeToBaseValue(NSRange);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TRANSFORM_MAC_H_
