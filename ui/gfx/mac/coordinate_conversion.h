// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_COORDINATE_CONVERSION_H_
#define UI_GFX_MAC_COORDINATE_CONVERSION_H_

#import <Foundation/Foundation.h>

#include "base/component_export.h"

namespace gfx {

class Point;
class Rect;

// Convert a gfx::Rect specified with the origin at the top left of the primary
// display into AppKit secreen coordinates (origin at the bottom left).
COMPONENT_EXPORT(GFX) NSRect ScreenRectToNSRect(const Rect& rect);

// Convert an AppKit NSRect with origin in the bottom left of the primary
// display into a gfx::Rect with origin at the top left of the primary display.
COMPONENT_EXPORT(GFX) Rect ScreenRectFromNSRect(const NSRect& point);

// Convert a gfx::Point specified with the origin at the top left of the primary
// display into AppKit screen coordinates (origin at the bottom left).
COMPONENT_EXPORT(GFX) NSPoint ScreenPointToNSPoint(const Point& point);

// Convert an AppKit NSPoint with origin in the bottom left of the primary
// display into a gfx::Point with origin at the top left of the primary display.
COMPONENT_EXPORT(GFX) Point ScreenPointFromNSPoint(const NSPoint& point);

}  // namespace gfx

#endif  // UI_GFX_MAC_COORDINATE_CONVERSION_H_
