// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/bubble_closer.h"

#import <AppKit/AppKit.h>

namespace ui {

BubbleCloser::BubbleCloser(NSWindow* window,
                           base::RepeatingClosure on_click_outside)
    : on_click_outside_(std::move(on_click_outside)), factory_(this) {
  // Capture a WeakPtr via NSObject. This allows the block to detect another
  // event monitor for the same event deleting |this|.
  WeakPtrNSObject* handle = factory_.handle();

  // Note that |window| will be retained when captured by the block below.
  // |this| is captured, but not retained.
  auto block = ^NSEvent*(NSEvent* event) {
    NSWindow* event_window = [event window];
    if ([event_window isSheet])
      return event;

    // Do not close the bubble if the event happened on a window with a
    // higher level.  For example, the content of a browser action bubble
    // opens a calendar picker window with NSPopUpMenuWindowLevel, and a
    // date selection closes the picker window, but it should not close
    // the bubble.
    if ([event_window level] > [window level])
      return event;

    // If the event is in |window|'s hierarchy, do not close the bubble.
    NSWindow* ancestor = event_window;
    while (ancestor) {
      if (ancestor == window)
        return event;
      ancestor = [ancestor parentWindow];
    }

    if (BubbleCloser* owner = WeakPtrNSObjectFactory<BubbleCloser>::Get(handle))
      owner->OnClickOutside();
    return event;
  };
  event_tap_ =
      [NSEvent addLocalMonitorForEventsMatchingMask:NSLeftMouseDownMask |
                                                    NSRightMouseDownMask
                                            handler:block];
}

BubbleCloser::~BubbleCloser() {
  [NSEvent removeMonitor:event_tap_];
}

void BubbleCloser::OnClickOutside() {
  on_click_outside_.Run();  // Note: May delete |this|.
}

}  // namespace ui
