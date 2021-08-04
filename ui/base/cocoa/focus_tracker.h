// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_FOCUS_TRACKER_H_
#define UI_BASE_COCOA_FOCUS_TRACKER_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"

// A class that handles saving and restoring focus.  An instance of
// this class snapshots the currently focused view when it is
// constructed, and callers can use restoreFocus to return focus to
// that view.  FocusTracker will not restore focus to views that are
// no longer in the view hierarchy or are not in the correct window.
COMPONENT_EXPORT(UI_BASE)
@interface FocusTracker : NSObject {
 @private
  base::scoped_nsobject<NSView> _focusedView;
}

// |window| is the window that we are saving focus for.  This
// method snapshots the currently focused view.
- (instancetype)initWithWindow:(NSWindow*)window;

// Attempts to restore focus to the snapshotted view.  Returns YES if
// focus was restored.  Will not restore focus if the view is no
// longer in the view hierarchy under |window|.
- (BOOL)restoreFocusInWindow:(NSWindow*)window;
@end

#endif  // UI_BASE_COCOA_FOCUS_TRACKER_H_