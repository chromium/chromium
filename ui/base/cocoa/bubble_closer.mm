// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/bubble_closer.h"

#import <AppKit/AppKit.h>

#include <memory>

#include "base/memory/weak_ptr.h"

namespace ui {

struct BubbleCloser::ObjCStorage {
  id __strong event_tap;
};

BubbleCloser::BubbleCloser(NSWindow* window,
                           base::RepeatingClosure on_click_outside)
    : on_click_outside_(std::move(on_click_outside)),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  // Capture a WeakPtr. This allows the block to detect another event monitor
  // for the same event deleting |this|.
  base::WeakPtr<BubbleCloser> weak_ptr = factory_.GetWeakPtr();

  // Note that |window| will be retained when captured by the block below.
  auto block = ^NSEvent*(NSEvent* event) {
    NSWindow* event_window = event.window;
    if (event_window.sheet) {
      return event;
    }

    // Do not close the bubble if the event happened on a window with a
    // higher level.  For example, the content of a browser action bubble
    // opens a calendar picker window with NSPopUpMenuWindowLevel, and a
    // date selection closes the picker window, but it should not close
    // the bubble.
    if (event_window.level > window.level) {
      return event;
    }

    // If the event is in |window|'s hierarchy, do not close the bubble.
    NSWindow* ancestor = event_window;
    while (ancestor) {
      if (ancestor == window)
        return event;
      ancestor = ancestor.parentWindow;
    }

    if (weak_ptr) {
      weak_ptr->OnClickOutside();
    }

    return event;
  };
  objc_storage_->event_tap =
      [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown |
                                                    NSEventMaskRightMouseDown
                                            handler:block];
}

BubbleCloser::~BubbleCloser() {
  [NSEvent removeMonitor:objc_storage_->event_tap];
}

void BubbleCloser::OnClickOutside() {
  on_click_outside_.Run();  // Note: May delete |this|.
}

}  // namespace ui
