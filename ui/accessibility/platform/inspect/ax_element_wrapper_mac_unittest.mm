// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"

// #include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

// Tests retrieving the position from an AXElementWrapper.
TEST(AXElementWrapperMacTest, Position) {
  // Test with an NSAccessibilityElement inside an AXElementWrapper.
  // With no frame set, we expect (0, 0).
  id node = [[NSAccessibilityElement alloc] init];
  AXElementWrapper wrapper(node);
  EXPECT_TRUE(NSEqualPoints(wrapper.Position(), NSZeroPoint));

  // Create a node with a frame, and confirm picking up the position (origin).
  NSRect frame = NSMakeRect(10, 20, 30, 40);
  node = [NSAccessibilityElement
      accessibilityElementWithRole:NSAccessibilityApplicationRole
                             frame:frame
                             label:@""
                            parent:nil];
  AXElementWrapper app_wrapper(node);
  EXPECT_TRUE(NSEqualPoints(app_wrapper.Position(), frame.origin));

  // Test with an AXUIElementRef inside an AXElementWrapper.
  AXUIElementRef element = AXUIElementCreateApplication(getpid());
  AXElementWrapper element_wrapper((__bridge id)element);
  EXPECT_TRUE(NSEqualPoints(element_wrapper.Position(), NSZeroPoint));
  CFRelease(element);

  // Anything other than an NSAccessibilityElement or AXUIElementRef inside
  // an AXElementWrapper should CHECK.
  id object = [[NSObject alloc] init];
  AXElementWrapper non_ax_wrapper(object);
  EXPECT_DEATH({ non_ax_wrapper.Position(); }, "");
}

}  // namespace ui
