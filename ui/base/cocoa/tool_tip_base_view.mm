// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "ui/base/cocoa/tool_tip_base_view.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"

// Below is the nasty tooltip stuff -- copied from WebKit's WebHTMLView.mm
// with minor modifications for code style and commenting.
//
//  The 'public' interface is -setToolTipAtMousePoint:. This differs from
// -setToolTip: in that the updated tooltip takes effect immediately,
//  without the user's having to move the mouse out of and back into the view.
//
// Unfortunately, doing this requires sending fake mouseEnter/Exit events to
// the view, which in turn requires overriding some internal tracking-rect
// methods (to keep track of its owner & userdata, which need to be filled out
// in the fake events.) --snej 7/6/09


/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

@implementation ToolTipBaseView {
  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id __weak _trackingRectOwner;
  raw_ptr<void, DanglingUntriaged> _trackingRectUserData;
  NSTrackingRectTag _lastToolTipTag;
  NSString* __strong _toolTip;
}

#ifndef MAC_OS_VERSION_13_0
#define MAC_OS_VERSION_13_0 130000
#endif

// Remove these methods once macOS 13 becomes the minimum deployment version
// (see comment in -setToolTipAtMousePoint: below). Consider moving the
// remainder into BaseView.
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0

// Any non-zero value will do, but using something recognizable might help us
// debug some day.
const NSTrackingRectTag kTrackingRectTag = 0xBADFACE;

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (NSTrackingRectTag)addTrackingRect:(NSRect)rect
                               owner:(id)owner
                            userData:(void *)data
                        assumeInside:(BOOL)assumeInside {
  DCHECK(_trackingRectOwner == nil);
  _trackingRectOwner = owner;
  _trackingRectUserData = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect
                                owner:(id)owner
                             userData:(void *)data
                         assumeInside:(BOOL)assumeInside
                       useTrackingNum:(int)tag {
  DCHECK(tag == 0 || tag == kTrackingRectTag);
  DCHECK(_trackingRectOwner == nil);
  _trackingRectOwner = owner;
  _trackingRectUserData = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (void)_addTrackingRects:(NSRect *)rects
                    owner:(id)owner
             userDataList:(void **)userDataList
         assumeInsideList:(BOOL *)assumeInsideList
             trackingNums:(NSTrackingRectTag *)trackingNums
                    count:(int)count {
  DCHECK(count == 1);
  DCHECK(trackingNums[0] == 0 || trackingNums[0] == kTrackingRectTag);
  DCHECK(_trackingRectOwner == nil);
  _trackingRectOwner = owner;
  _trackingRectUserData = userDataList[0];
  trackingNums[0] = kTrackingRectTag;
}

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (void)removeTrackingRect:(NSTrackingRectTag)tag {
  if (tag == 0)
    return;

  if (tag == kTrackingRectTag) {
    _trackingRectOwner = nil;
    return;
  }

  if (tag == _lastToolTipTag) {
    [super removeTrackingRect:tag];
    _lastToolTipTag = 0;
    return;
  }

  // If any other tracking rect is being removed, we don't know how it was
  // created and it's possible there's a leak involved (see Radar 3500217).
  NOTREACHED();
}

// Override of (apparently) a private NSView method(!)
- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count {
  for (int i = 0; i < count; ++i) {
    NSTrackingRectTag tag = tags[i];
    if (tag == 0)
      continue;
    DCHECK(tag == kTrackingRectTag);
    _trackingRectOwner = nil;
  }
}

// Sends a fake NSEventTypeMouseExited event to the view for its current
// tracking rect.
- (void)_sendToolTipMouseExited {
  // Nothing matters except window, trackingNumber, and userData.
  NSEvent* fakeEvent =
      [NSEvent enterExitEventWithType:NSEventTypeMouseExited
                             location:NSZeroPoint
                        modifierFlags:0
                            timestamp:NSApp.currentEvent.timestamp
                         windowNumber:self.window.windowNumber
                              context:nullptr
                          eventNumber:0
                       trackingNumber:kTrackingRectTag
                             userData:_trackingRectUserData];
  [_trackingRectOwner mouseExited:fakeEvent];
}

// Sends a fake NSEventTypeMouseEntered event to the view for its current
// tracking rect.
- (void)_sendToolTipMouseEntered {
  NSInteger windowNumber = self.window.windowNumber;

  // Only send a fake mouse enter if the mouse is actually over the window,
  // versus over a window which overlaps it (see http://crbug.com/883269).
  if ([NSWindow windowNumberAtPoint:NSEvent.mouseLocation
          belowWindowWithWindowNumber:0] != windowNumber) {
    return;
  }

  // Nothing matters except window, trackingNumber, and userData.
  NSEvent* fakeEvent =
      [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
                             location:NSZeroPoint
                        modifierFlags:0
                            timestamp:NSApp.currentEvent.timestamp
                         windowNumber:windowNumber
                              context:nullptr
                          eventNumber:0
                       trackingNumber:kTrackingRectTag
                             userData:_trackingRectUserData];
  [_trackingRectOwner mouseEntered:fakeEvent];
}

#endif  // MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0

// Sets the view's current tooltip, to be displayed at the current mouse
// location. (This does not make the tooltip appear -- as usual, it only
// appears after a delay.) Pass null to remove the tooltip.
- (void)setToolTipAtMousePoint:(NSString *)string {
  NSString* toolTip = string.length == 0 ? nil : string;
  if ((toolTip && _toolTip && [toolTip isEqualToString:_toolTip]) ||
      (!toolTip && !_toolTip)) {
    return;
  }

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0
  if (_toolTip) {
    [self _sendToolTipMouseExited];
  }
#endif  // MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0

  _toolTip = [toolTip copy];

  // It appears that as of macOS 13, tooltips are no longer set up with
  // calls to addTrackingRect:... or removeTrackingRect:. As a result,
  // _trackingRectOwner remains nil, which means the calls to
  // [_trackingRectOwner mouseEntered:] in _sendToolTipMouseEntered and
  // [_trackingRectOwner mouseExited:] in _sendToolTipMouseExited do nothing.
  // It looks like this doesn't affect tooltip display, but the call to
  // _sendToolTipMouseExited no longer orders it out. Therefore, when the user
  // moves the mouse away from a tooltip on Ventura, the call to
  // setToolTipAtMousePoint:nil initiated by the target view leaves the
  // tooltip onscreen.
  //
  // The logic below was
  //
  //   if (tooltip) {
  //     [self removeAllTooltips];
  //     ...
  //   }
  //
  // By moving the call to -removeAllTooltips outside of the conditional,
  // we can ensure any visible tooltip will be removed from the screen.
  // See crbug.com/1409942.
  //
  // The strategy of removing all tooltips rather than the single one that
  // was added comes from WebKit, like the rest of the code here. It
  // apparently works around some AppKit bug.
  [self removeAllToolTips];
  if (toolTip) {
    NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
    _lastToolTipTag = [self addToolTipRect:wideOpenRect
                                     owner:self
                                  userData:nullptr];
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0
    [self _sendToolTipMouseEntered];
#endif  // MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_13_0
  }
}

// NSView calls this to get the text when displaying the tooltip.
- (NSString *)view:(NSView *)view
  stringForToolTip:(NSToolTipTag)tag
             point:(NSPoint)point
          userData:(void *)data {
  return [_toolTip copy];
}

@end
