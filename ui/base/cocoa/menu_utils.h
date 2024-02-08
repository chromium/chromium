// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_MENU_UTILS_H_
#define UI_BASE_COCOA_MENU_UTILS_H_

#include <Foundation/Foundation.h>

#include "base/component_export.h"
#include "ui/base/interaction/element_identifier.h"

@class NSEvent;
@class NSMenu;
@class NSView;
@class NSWindow;

namespace gfx {
class Point;
}

namespace ui {

// Returns an appropriate event (with a location) suitable for showing a context
// menu. Uses [NSApp currentEvent] if it's a non-nil mouse click event,
// otherwise creates an autoreleased dummy event located at `anchor`, where
// `anchor` is in screen coordinates.
COMPONENT_EXPORT(UI_BASE)
NSEvent* EventForPositioningContextMenu(const gfx::Point& anchor,
                                        NSWindow* window);

// Same as above, but `position` is in window coordinates relative to `window`
// instead.
COMPONENT_EXPORT(UI_BASE)
NSEvent* EventForPositioningContextMenuRelativeToWindow(const NSPoint& position,
                                                        NSWindow* window);

// Pops up `menu` using `event`, targeting `view`. If `allow_nested_tasks` is
// true, tasks (such as IPCs) will be allowed to execute while the menu is being
// displayed. If specified, `context` is used to inform ElementTrackerMac of the
// menu being shown.
COMPONENT_EXPORT(UI_BASE)
void ShowContextMenu(NSMenu* menu,
                     NSEvent* event,
                     NSView* view,
                     bool allow_nested_tasks,
                     ElementContext context = ElementContext());

}  // namespace ui

#endif  // UI_BASE_COCOA_MENU_UTILS_H_
