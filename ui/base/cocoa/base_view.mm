// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/base_view.h"

#include "base/check_op.h"
#include "base/mac/mac_util.h"

NSString* kViewDidBecomeFirstResponder =
    @"Chromium.kViewDidBecomeFirstResponder";
NSString* kSelectionDirection = @"Chromium.kSelectionDirection";

@implementation BaseView

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self enableTracking];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self enableTracking];
  }
  return self;
}

- (void)dealloc {
  [self disableTracking];
  [super dealloc];
}

- (void)enableTracking {
  if (_trackingArea.get())
    return;

  NSTrackingAreaOptions trackingOptions = NSTrackingMouseEnteredAndExited |
                                          NSTrackingMouseMoved |
                                          NSTrackingActiveAlways |
                                          NSTrackingInVisibleRect;
  _trackingArea.reset([[CrTrackingArea alloc] initWithRect:NSZeroRect
                                                   options:trackingOptions
                                                     owner:self
                                                  userInfo:nil]);
  [self addTrackingArea:_trackingArea.get()];
}

- (void)disableTracking {
  if (_trackingArea.get()) {
    [self removeTrackingArea:_trackingArea.get()];
    _trackingArea.reset();
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];

  // NSTrackingInVisibleRect doesn't work correctly with Lion's window
  // resizing (See https://crbug.com/176725 and
  // http://openradar.appspot.com/radar?id=2773401). It also doesn't work
  // correctly when the window enters fullscreen
  // (See https://crbug.com/170058).
  //
  // Work around it by reinstalling the tracking area after the window resizes
  // or enters fullscreen. This AppKit bug is fixed on High Sierra, so we only
  // apply this workaround on 10.12 or earlier.
  if (base::mac::IsAtMostOS10_12()) {
    [self disableTracking];
    [self enableTracking];
  }
}

- (void)handleLeftMouseUp:(NSEvent*)theEvent {
  DCHECK_EQ([theEvent type], NSLeftMouseUp);
  _dragging = NO;
  if (!_pendingExitEvent)
    return;

  NSEvent* exitEvent =
      [NSEvent enterExitEventWithType:NSMouseExited
                             location:[theEvent locationInWindow]
                        modifierFlags:[theEvent modifierFlags]
                            timestamp:[theEvent timestamp]
                         windowNumber:[theEvent windowNumber]
                              context:[theEvent context]
                          eventNumber:[_pendingExitEvent eventNumber]
                       trackingNumber:[_pendingExitEvent trackingNumber]
                             userData:[_pendingExitEvent userData]];
  [self mouseEvent:exitEvent];
  _pendingExitEvent.reset();
}

- (void)mouseEvent:(NSEvent*)theEvent {
  // This method left intentionally blank.
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  // The default implementation of this method does not handle any key events.
  // Derived classes should return kEventHandled if they handled an event,
  // otherwise it will be forwarded on to |super|.
  return kEventNotHandled;
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  // This method left intentionally blank.
}

- (void)tabletEvent:(NSEvent*)theEvent {
  // This method left intentionally blank.
}

- (void)mouseDown:(NSEvent*)theEvent {
  _dragging = YES;
  [self mouseEvent:theEvent];
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
  [self handleLeftMouseUp:theEvent];
}

- (void)rightMouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)rightMouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  [self mouseEvent:theEvent];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  if (_pendingExitEvent) {
    _pendingExitEvent.reset();
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)mouseExited:(NSEvent*)theEvent {
  // The tracking area will send an exit event even during a drag, which isn't
  // how the event flow for drags should work. This stores the exit event, and
  // sends it when the drag completes instead.
  if (_dragging) {
    _pendingExitEvent.reset([theEvent retain]);
    return;
  }

  [self mouseEvent:theEvent];
}

- (void)keyDown:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super keyDown:theEvent];
}

- (void)keyUp:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super keyUp:theEvent];
}

- (void)pressureChangeWithEvent:(NSEvent*)theEvent {
  NSInteger newStage = [theEvent stage];
  if (_pressureEventStage == newStage)
    return;

  // Call the force touch event when the stage reaches 2, which is the value
  // for force touch.
  if (newStage == 2) {
    [self forceTouchEvent:theEvent];
  }
  _pressureEventStage = newStage;
}

- (void)flagsChanged:(NSEvent*)theEvent {
  if ([self keyEvent:theEvent] != kEventHandled)
    [super flagsChanged:theEvent];
}

- (gfx::Rect)flipNSRectToRect:(NSRect)rect {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y(NSHeight([self bounds]) - new_rect.bottom());
  return new_rect;
}

- (NSRect)flipRectToNSRect:(gfx::Rect)rect {
  NSRect new_rect(NSRectFromCGRect(rect.ToCGRect()));
  new_rect.origin.y = NSHeight([self bounds]) - NSMaxY(new_rect);
  return new_rect;
}

- (void)tabletProximity:(NSEvent*)theEvent {
  [self tabletEvent:theEvent];
}

@end
