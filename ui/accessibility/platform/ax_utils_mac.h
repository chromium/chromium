// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_

#import <Cocoa/Cocoa.h>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

class AXPlatformNodeCocoa;

// An AXTextMarker is used by applications like Chrome to store a position in
// the accessibility tree's text representation. It is a data structure whose
// contents are opaque to the system but whose allocation and deallocation is
// managed by it. The contents are interpreted by the application that created
// it.
// An AXTextMarkerRange is a pair of AXTextMarkers. The data in each of the
// two AXTextMarkers is provided by the application, but similar to an
// AXTextMarker its memory storage is managed by the system.

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

// Returns the AXTextMarker representing the given AXNodePosition.
AX_EXPORT id AXPositionToAXTextMarker(AXPlatformNodeDelegate::AXPosition);

// Returns the AXTextMarkerRange representing the given AXRange.
AX_EXPORT id AXRangeToAXTextMarkerRange(AXPlatformNodeDelegate::AXRange);

// Returns the AXTextMarker representing the position within the given node.
AX_EXPORT id AXTextMarkerFrom(const AXPlatformNodeCocoa* anchor,
                              int offset,
                              ax::mojom::TextAffinity affinity);

// Returns the AXTextMarkerRange representing the given AXTextMarker objects.
AX_EXPORT id AXTextMarkerRangeFrom(id anchor_textmarker, id focus_textmarker);

// Returns the start text marker from the given AXTextMarkerRange.
AX_EXPORT id AXTextMarkerRangeStart(id text_marker_range);

// Returns the end text marker from the given AXTextMarkerRange.
AX_EXPORT id AXTextMarkerRangeEnd(id text_marker_range);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_UTILS_MAC_H_
