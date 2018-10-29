// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/base/ui_base_export.h"

// A class that handles saving and restoring focus.  An instance of
// this class snapshots the currently focused view when it is
// constructed, and callers can use restoreFocus to return focus to
// that view.  FocusTracker will not restore focus to views that are
// no longer in the view hierarchy or are not in the correct window.
UI_BASE_EXPORT
@interface FocusTracker : NSObject {
 @private
  base::scoped_nsobject<NSView> focusedView_;
}

// |window| is the window that we are saving focus for.  This
// method snapshots the currently focused view.
- (instancetype)initWithWindow:(NSWindow*)window;

// Attempts to restore focus to the snapshotted view.  Returns YES if
// focus was restored.  Will not restore focus if the view is no
// longer in the view hierarchy under |window|.
- (BOOL)restoreFocusInWindow:(NSWindow*)window;
@end
