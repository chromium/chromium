// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_COCOA_HELPER_H_
#define UI_BASE_TEST_COCOA_HELPER_H_

#include <set>

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#import "base/strings/sys_string_conversions.h"
#include "testing/platform_test.h"

// CocoaTestHelperWindow behaves differently from a regular NSWindow in the
// following ways:
// - It allows -isKeyWindow to be manipulated to test things like focus rings
//   (which background windows won't normally display).
// - It ignores its real occlusion state and returns a value based on
//   pretendIsOccluded.
// - It ignores the system setting for full keyboard access and returns a value
//   based on pretendFullKeyboardAccessIsEnabled.
@interface CocoaTestHelperWindow : NSWindow

// Value to return for -isKeyWindow.
@property(nonatomic) BOOL pretendIsKeyWindow;

// Value to return for -occlusionState. Setting posts a
// NSWindowDidChangeOcclusionStateNotification.
@property(nonatomic) BOOL pretendIsOccluded;

// Value to return for -isOnActiveSpace. Posts
// NSWorkspaceActiveSpaceDidChangeNotification when set.
@property(nonatomic) BOOL pretendIsOnActiveSpace;

// Whether to handle the key view loop as if full keyboard access is enabled.
@property(nonatomic) BOOL pretendFullKeyboardAccessIsEnabled;

// Whether to use or ignore the default contraints for window sizing and
// placement.
@property(nonatomic) BOOL useDefaultConstraints;

// All of the window's valid key views, in order.
@property(nonatomic, readonly) NSArray<NSView*>* validKeyViews;

// Init a borderless non-deferred window with a backing store.
- (instancetype)initWithContentRect:(NSRect)contentRect;

// Init with a default frame.
- (instancetype)init;

// Sets the responder passed in as first responder, and sets the window
// so that it will return "YES" if asked if it key window. It does not actually
// make the window key.
- (void)makePretendKeyWindowAndSetFirstResponder:(NSResponder*)responder;

// Clears the first responder duty for the window and returns the window
// to being non-key.
- (void)clearPretendKeyWindowAndFirstResponder;

- (BOOL)isKeyWindow;

- (NSWindowOcclusionState)occlusionState;

@end

namespace ui {

// A test class that all tests that depend on AppKit should inherit from.
// Sets up paths correctly, and makes sure that any windows created in the test
// are closed down properly by the test.
class CocoaTest : public PlatformTest {
 public:
  CocoaTest();
  ~CocoaTest() override;

  // Must be called by subclasses that override TearDown. We verify that it
  // is called in our destructor. Takes care of making sure that all windows
  // are closed off correctly. If your tests open windows, they must be sure
  // to close them before CocoaTest::TearDown is called. A standard way of doing
  // this would be to create them in SetUp (after calling CocoaTest::Setup) and
  // then close them in TearDown before calling CocoaTest::TearDown.
  void TearDown() override;

  // Retuns a test window that can be used by views and other UI objects
  // as part of their tests. Is created lazily, and will be closed correctly
  // in CocoaTest::TearDown. Note that it is a CocoaTestHelperWindow which
  // has special handling for being Key.
  CocoaTestHelperWindow* test_window();

 protected:
  // Allows subclasses to do initialization before calling through to the base
  // class's initialization.
  void Init();

 private:
  // Return a set of currently open windows. Avoiding NSArray so
  // contents aren't retained, the pointer values can only be used for
  // comparison purposes.  Using std::set to make progress-checking
  // convenient.
  static std::set<NSWindow*> ApplicationWindows();

  // Return a set of windows which are in |ApplicationWindows()| but
  // not |initial_windows_|.
  std::set<NSWindow*> WindowsLeft();

  bool called_tear_down_;
  base::mac::ScopedNSAutoreleasePool pool_;

  // Windows which existed at the beginning of the test.
  std::set<NSWindow*> initial_windows_;

  // Strong. Lazily created. This isn't wrapped in a scoped_nsobject because
  // we want to call [close] to destroy it rather than calling [release]. We
  // want to verify that [close] is actually removing our window and that it's
  // not hanging around because releaseWhenClosed was set to "no" on the window.
  // It isn't wrapped in a different wrapper class to close it because we
  // need to close it at a very specific time; just before we enter our clean
  // up loop in TearDown.
  CocoaTestHelperWindow* test_window_;
};

}  // namespace ui

// A macro defining a standard set of tests to run on a view. Since we can't
// inherit tests, this macro saves us a lot of duplicate code. Handles simply
// displaying the view to make sure it won't crash, as well as removing it
// from a window. All tests that work with NSView subclasses and/or
// NSViewController subclasses should use it.
#define TEST_VIEW(test_fixture, test_view)                      \
  TEST_F(test_fixture, test_fixture##_TestViewMacroAddRemove) { \
    base::scoped_nsobject<NSView> view([test_view retain]);     \
    EXPECT_EQ([test_window() contentView], [view superview]);   \
    [view removeFromSuperview];                                 \
    EXPECT_FALSE([view superview]);                             \
  }                                                             \
  TEST_F(test_fixture, test_fixture##_TestViewMacroDisplay) {   \
    [test_view display];                                        \
  }

// A macro which determines the proper float epsilon for a CGFloat.
#if CGFLOAT_IS_DOUBLE
#define CGFLOAT_EPSILON DBL_EPSILON
#else
#define CGFLOAT_EPSILON FLT_EPSILON
#endif

// A macro which which determines if two CGFloats are equal taking a
// proper epsilon into consideration.
#define CGFLOAT_EQ(expected, actual)         \
  (actual >= (expected - CGFLOAT_EPSILON) && \
   actual <= (expected + CGFLOAT_EPSILON))

// A test support macro which ascertains if two CGFloats are equal.
#define EXPECT_CGFLOAT_EQ(expected, actual) \
  EXPECT_TRUE(CGFLOAT_EQ(expected, actual)) << expected << " != " << actual

// A test support macro which compares two NSRects for equality taking
// the float epsilon into consideration.
#define EXPECT_NSRECT_EQ(expected, actual)                          \
  EXPECT_TRUE(CGFLOAT_EQ(expected.origin.x, actual.origin.x) &&     \
              CGFLOAT_EQ(expected.origin.y, actual.origin.y) &&     \
              CGFLOAT_EQ(expected.size.width, actual.size.width) && \
              CGFLOAT_EQ(expected.size.height, actual.size.height)) \
      << "Rects do not match: "                                     \
      << base::SysNSStringToUTF8(NSStringFromRect(expected))        \
      << " != " << base::SysNSStringToUTF8(NSStringFromRect(actual))

#endif  // UI_BASE_TEST_COCOA_HELPER_H_
