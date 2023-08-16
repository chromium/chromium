// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cocoa_base_utils.h"

#import <objc/objc-class.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"
#include "ui/events/event_constants.h"
#import "ui/events/test/cocoa_test_event_utils.h"

// We provide a donor class with a specially modified |modifierFlags|
// implementation that we swap with NSEvent's. This is because we can't create a
// NSEvent that represents a middle click with modifiers.
@interface TestEvent : NSObject
@end
@implementation TestEvent
- (NSUInteger)modifierFlags {
  return NSEventModifierFlagShift;
}
@end

namespace ui {

namespace {

class CocoaBaseUtilsTest : public CocoaTest {
};

TEST_F(CocoaBaseUtilsTest, WindowOpenDispositionFromNSEvent) {
  // Left Click = same tab.
  NSEvent* me =
      cocoa_test_event_utils::MouseEventWithType(NSEventTypeLeftMouseUp, 0);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            WindowOpenDispositionFromNSEvent(me));

  // Middle Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSEventTypeOtherMouseUp, 0);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            WindowOpenDispositionFromNSEvent(me));

  // Shift+Middle Click = new foreground tab.
  {
    base::apple::ScopedObjCClassSwizzler swizzler(
        [NSEvent class], [TestEvent class], @selector(modifierFlags));
    me = cocoa_test_event_utils::MouseEventWithType(NSEventTypeOtherMouseUp,
                                                    NSEventModifierFlagShift);
    EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
              WindowOpenDispositionFromNSEvent(me));
  }

  // Cmd+Left Click = new background tab.
  me = cocoa_test_event_utils::MouseEventWithType(NSEventTypeLeftMouseUp,
                                                  NSEventModifierFlagCommand);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            WindowOpenDispositionFromNSEvent(me));

  // Cmd+Shift+Left Click = new foreground tab.
  me = cocoa_test_event_utils::MouseEventWithType(
      NSEventTypeLeftMouseUp,
      NSEventModifierFlagCommand | NSEventModifierFlagShift);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            WindowOpenDispositionFromNSEvent(me));

  // Shift+Left Click = new window
  me = cocoa_test_event_utils::MouseEventWithType(NSEventTypeLeftMouseUp,
                                                  NSEventModifierFlagShift);
  EXPECT_EQ(WindowOpenDisposition::NEW_WINDOW,
            WindowOpenDispositionFromNSEvent(me));
}

}  // namespace

}  // namespace ui
