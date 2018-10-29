// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "ui/base/cocoa/focus_window_set.h"

namespace ui {

namespace {

// This attempts to match OS X's native behavior, namely that a window
// is only ever deminiaturized if ALL windows on ALL workspaces are
// miniaturized.
void FocusWindowSetHelper(const std::set<gfx::NativeWindow>& windows,
                          bool allow_workspace_switch,
                          bool visible_windows_only) {
  NSArray* ordered_windows = [NSApp orderedWindows];
  NSWindow* frontmost_window = nil;
  NSWindow* frontmost_window_all_spaces = nil;
  NSWindow* frontmost_miniaturized_window = nil;
  bool all_miniaturized = true;
  for (int i = [ordered_windows count] - 1; i >= 0; i--) {
    NSWindow* win = ordered_windows[i];
    if (windows.find(win) == windows.end())
      continue;
    if ([win isMiniaturized]) {
      frontmost_miniaturized_window = win;
    } else if (!visible_windows_only || [win isVisible]) {
      all_miniaturized = false;
      frontmost_window_all_spaces = win;
      if ([win isOnActiveSpace]) {
        // Raise the old |frontmost_window| (if any). The topmost |win| will be
        // raised with makeKeyAndOrderFront: below.
        [frontmost_window orderFront:nil];
        frontmost_window = win;
      }
    }
  }
  if (all_miniaturized && frontmost_miniaturized_window) {
    DCHECK(!frontmost_window);
    // Note the call to makeKeyAndOrderFront: will deminiaturize the window.
    frontmost_window = frontmost_miniaturized_window;
  }
  // If we couldn't find one on the active space, consider all spaces.
  if (allow_workspace_switch && !frontmost_window)
    frontmost_window = frontmost_window_all_spaces;

  if (frontmost_window) {
    [frontmost_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  }
}

}  // namespace

void FocusWindowSet(const std::set<gfx::NativeWindow>& windows) {
  FocusWindowSetHelper(windows, true, true);
}

void FocusWindowSetOnCurrentSpace(const std::set<gfx::NativeWindow>& windows) {
  // This callback runs before AppKit picks its own window to
  // deminiaturize, so we get to pick one from the right set. Limit to
  // the windows on the current workspace. Otherwise we jump spaces
  // haphazardly.
  //
  // Also consider both visible and hidden windows; this call races
  // with the system unhiding the application. http://crbug.com/368238
  //
  // NOTE: If this is called in the
  // applicationShouldHandleReopen:hasVisibleWindows: hook when
  // clicking the dock icon, and that caused OS X to begin switch
  // spaces, isOnActiveSpace gives the answer for the PREVIOUS
  // space. This means that we actually raise and focus the wrong
  // space's windows, leaving the new key window off-screen. To detect
  // this, check if the key window is on the active space prior to
  // calling.
  //
  // Also, if we decide to deminiaturize a window during a space switch,
  // that can switch spaces and then switch back. Fortunately, this only
  // happens if, say, space 1 contains an app, space 2 contains a
  // miniaturized browser. We click the icon, OS X switches to space 1,
  // we deminiaturize the browser, and that triggers switching back.
  //
  // TODO(davidben): To limit those cases, consider preferentially
  // deminiaturizing a window on the current space.
  FocusWindowSetHelper(windows, false, false);
}

}  // namespace ui
