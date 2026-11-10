// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/drag_coordinator.h"

#import <AppKit/AppKit.h>

#include "base/check_op.h"

namespace ui {

namespace {
NSView* __weak gDragSessionInProgressResponsibleView;
}  // namespace

bool IsDragSessionInitiationAllowed() {
  CHECK_EQ(NSRunLoop.currentRunLoop, NSRunLoop.mainRunLoop);
  return !gDragSessionInProgressResponsibleView &&
         NSRunLoop.currentRunLoop.currentMode != NSEventTrackingRunLoopMode;
}

void SetDragSessionInProgress(NSView* responsible_view) {
  if (responsible_view) {
    CHECK_EQ(gDragSessionInProgressResponsibleView, nil);
  }
  gDragSessionInProgressResponsibleView = responsible_view;
}

void CancelDragSessionInProgressForView(NSView* owning_view) {
  if (!gDragSessionInProgressResponsibleView) {
    return;
  }
  // FYI, it is not clear from the docs, but -isDescendantOf: returns YES for
  // the query of a view being a descendent of itself.
  if ([gDragSessionInProgressResponsibleView isDescendantOf:owning_view]) {
    // There is no API to cancel a drag on the Mac.
    //
    // From the mists of time through macOS 14, AppKit called into CoreDrag from
    // all over the place. Starting with macOS 15, it started funneling direct
    // CoreDrag calls through the centralized _NSDragManager wrapper class as
    // class methods (+cancelDrag, +isDragging, +hideDragWindow,
    // +showDragWindow, etc).
    //
    // Chromium must support earlier than that OS, and (in any case) SPI usage
    // should be avoided when possible. Investigation reveals that
    // -[NSApplication(NSApplicationAccessibility) accessibilityPerformAction:]
    // (on OS X 10.10 and later) handles exactly one action: an unexported
    // constant NSAccessibilityCancelDragAction (aka @"AXCancelDragOperation")
    // to cancel a drag. This is deprecated accessibility API, but there doesn't
    // seem to be a way to get at this via non-deprecated API.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp accessibilityPerformAction:@"AXCancelDragOperation"];
#pragma clang diagnostic pop
  }
}

}  // namespace ui
