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
  // Return a set of currently open windows. Avoiding NSArray so
  // contents aren't retained, the pointer values can only be used for
  // comparison purposes.  Using std::set to make progress-checking
  // convenient.
  static std::set<NSWindow*> ApplicationWindows();

  // Return a set of windows which are in |ApplicationWindows()| but
  // not |initial_windows_|.
  std::set<NSWindow*> WindowsLeft();

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
  CocoaTestHelperWindow* test_window_ = nil;
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
