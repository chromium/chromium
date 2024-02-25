// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_COCOA_HELPER_H_
#define UI_BASE_TEST_COCOA_HELPER_H_

#import <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

#include <memory>

#import "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#include "testing/platform_test.h"
#include "ui/display/screen.h"

// CocoaTestHelperWindow behaves differently from a regular NSWindow in the
// following ways:
// - It allows -isKeyWindow to be manipulated to test things like focus rings
//   (which background windows won't normally display).
// - It ignores the system setting for full keyboard access and returns a value
//   based on pretendFullKeyboardAccessIsEnabled.
@interface CocoaTestHelperWindow : NSWindow

// Value to return for -isKeyWindow.
@property(nonatomic) BOOL pretendIsKeyWindow;

// Value to return for -isOnActiveSpace. Posts
// NSWorkspaceActiveSpaceDidChangeNotification when set.
@property(nonatomic) BOOL pretendIsOnActiveSpace;

// Whether to handle the key view loop as if full keyboard access is enabled.
@property(nonatomic) BOOL pretendFullKeyboardAccessIsEnabled;

// Whether to use or ignore the default constraints for window sizing and
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

@end

namespace ui {

class CocoaTestHelper {
 public:
  CocoaTestHelper();
  virtual ~CocoaTestHelper();

  void MarkCurrentWindowsAsInitial();

  // Returns a test window that can be used by views and other UI objects
  // as part of their tests. Is created lazily, and will be closed correctly
  // in CocoaTest::TearDown. Note that it is a CocoaTestHelperWindow which
  // has special handling for being Key.
  CocoaTestHelperWindow* test_window();

 private:
  // A vector to hold a list of weak NSWindow pointers. Used as the container
  // for lists of weak pointers, because putting pointers that can change their
  // values to nil inside a set would break the hash.
  using WeakWindowVector = std::vector<NSWindow * __weak>;

  // Returns a vector of currently open windows.
  WeakWindowVector ApplicationWindows();

  // Returns a vector of windows which are in `ApplicationWindows()` but not
  // `initial_windows_`.
  WeakWindowVector WindowsLeft();

  display::ScopedNativeScreen screen_;

  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool pool_;

  // Windows which existed at the beginning of the test.
  WeakWindowVector initial_windows_;

  CocoaTestHelperWindow* __strong test_window_;
};

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

  CocoaTestHelperWindow* test_window() { return helper_->test_window(); }

 protected:
  void MarkCurrentWindowsAsInitial();

 private:
  std::unique_ptr<CocoaTestHelper> helper_;
};

}  // namespace ui

#endif  // UI_BASE_TEST_COCOA_HELPER_H_
