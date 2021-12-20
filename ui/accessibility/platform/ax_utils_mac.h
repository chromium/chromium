// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// Uses a system API to verify that the given object is an AXTextMarker object.
AX_EXPORT bool IsAXTextMarker(id text_marker);

// Uses a system API to verify that the given object is an AXTextMarkerRange
// object.
AX_EXPORT bool IsAXTextMarkerRange(id marker_range);

// Returns the AXNodePosition representing the given AXTextMarker.
AX_EXPORT AXPlatformNodeDelegate::AXPosition AXTextMarkerToAXPosition(
    id text_marker);

// Returns the AXRange representing the given AXTextMarkerRange.
AX_EXPORT AXPlatformNodeDelegate::AXRange AXTextMarkerRangeToAXRange(
    id marker_range);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_
