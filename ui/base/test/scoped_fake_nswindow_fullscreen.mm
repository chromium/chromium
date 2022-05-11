// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/base_window.h"

// Donates a testing implementation of [NSWindow toggleFullScreen:].
@interface ToggleFullscreenDonorForWindow : NSObject
@end

namespace {

ui::test::ScopedFakeNSWindowFullscreen::Impl* g_fake_fullscreen_impl = nullptr;

uint64_t g_instance_count = 0;

}  // namespace

namespace ui::test {

class ScopedFakeNSWindowFullscreen::Impl {
 public:
  Impl()
      : toggle_fullscreen_swizzler_([NSWindow class],
                                    [ToggleFullscreenDonorForWindow class],
                                    @selector(toggleFullScreen:)),
        style_mask_swizzler_([NSWindow class],
                             [ToggleFullscreenDonorForWindow class],
                             @selector(styleMask)),
        set_style_mask_swizzler_([NSWindow class],
                                 [ToggleFullscreenDonorForWindow class],
                                 @selector(setStyleMask:)) {}

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() {
    // If there's a pending transition, it means there's a task in the queue to
    // complete it, referencing |this|.
    DCHECK(!is_in_transition_);
  }

  void ToggleFullscreenForWindow(NSWindow* window) {
    DCHECK(!is_in_transition_);
    if (window_ == nil) {
      StartEnterFullscreen(window);
    } else if (window_ == window) {
      StartExitFullscreen();
    } else {
      // Another window is fullscreen.
      NOTREACHED();
    }
  }

  void OriginalSetStyleMask(id receiver, SEL selector, NSUInteger mask) {
    return set_style_mask_swizzler_.InvokeOriginal<void, NSUInteger>(
        receiver, selector, mask);
  }

  NSUInteger StyleMaskForWindow(NSWindow* window) {
    auto actual_style_mask = style_mask_swizzler_.InvokeOriginal<NSUInteger>(
        window, @selector(styleMask));
    if (window_ != window || !style_as_fullscreen_)
      return actual_style_mask;

    // The window should never "actually" be fullscreen.
    DCHECK_EQ(0u, actual_style_mask & NSWindowStyleMaskFullScreen);
    return actual_style_mask | NSWindowStyleMaskFullScreen;
  }

  void StartEnterFullscreen(NSWindow* window) {
    // If the window cannot go fullscreen, do nothing.
    if (!([window collectionBehavior] &
          NSWindowCollectionBehaviorFullScreenPrimary)) {
      return;
    }

    id<NSWindowDelegate> delegate = [window delegate];

    // Nothing is currently fullscreen. Make this window fullscreen.
    window_ = window;
    is_in_transition_ = true;
    frame_before_fullscreen_ = [window frame];
    NSSize fullscreen_content_size =
        [window contentRectForFrameRect:[[window screen] frame]].size;
    if ([delegate respondsToSelector:@selector(window:
                                         willUseFullScreenContentSize:)]) {
      fullscreen_content_size = [delegate window:window
                    willUseFullScreenContentSize:fullscreen_content_size];
    }
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillEnterFullScreenNotification
                      object:window];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillStartLiveResizeNotification
                      object:window];
    DCHECK(base::CurrentUIThread::IsSet());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Impl::FinishEnterFullscreen, base::Unretained(this),
                       fullscreen_content_size));
  }

  void FinishEnterFullscreen(NSSize fullscreen_content_size) {
    // The frame should not have changed during the transition.
    DCHECK(NSEqualRects(frame_before_fullscreen_, [window_ frame]));

    // Style mask must be set first because -[NSWindow frame] may be different
    // depending on NSWindowStyleMaskFullScreen. Don't call -[NSWindow
    // setStyleMask:] since that will trigger a fullscreen transition that
    // bypasses the swizzled -toggleFullScreen: method. Instead, fake it.
    style_as_fullscreen_ = true;

    // The origin doesn't matter, NSWindowStyleMaskFullScreen means the origin
    // will be adjusted.
    NSRect target_fullscreen_frame = [window_
        frameRectForContentRect:NSMakeRect(0, 0, fullscreen_content_size.width,
                                           fullscreen_content_size.height)];
    [window_ setFrame:target_fullscreen_frame display:YES animate:NO];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowDidEndLiveResizeNotification
                      object:window_];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowDidEnterFullScreenNotification
                      object:window_];
    // Store the actual frame because we check against it when exiting.
    frame_during_fullscreen_ = [window_ frame];
    is_in_transition_ = false;
  }

  void StartExitFullscreen() {
    is_in_transition_ = true;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillExitFullScreenNotification
                      object:window_];

    DCHECK(base::CurrentUIThread::IsSet());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Impl::FinishExitFullscreen, base::Unretained(this)));
  }

  void FinishExitFullscreen() {
    // The bounds may have changed during the transition. Check for this before
    // setting the style mask because -[NSWindow frame] may be different
    // depending on NSWindowStyleMaskFullScreen.
    bool no_frame_change_during_fullscreen =
        NSEqualRects(frame_during_fullscreen_, [window_ frame]);
    // Set the original frame after setting the style mask.
    if (no_frame_change_during_fullscreen)
      [window_ setFrame:frame_before_fullscreen_ display:YES animate:NO];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowDidExitFullScreenNotification
                      object:window_];
    window_ = nil;
    is_in_transition_ = false;
    style_as_fullscreen_ = false;
  }

  bool is_in_transition() { return is_in_transition_; }

 private:
  base::mac::ScopedObjCClassSwizzler toggle_fullscreen_swizzler_;
  base::mac::ScopedObjCClassSwizzler style_mask_swizzler_;
  base::mac::ScopedObjCClassSwizzler set_style_mask_swizzler_;

  // The currently fullscreen window.
  NSWindow* window_ = nil;
  NSRect frame_before_fullscreen_;
  NSRect frame_during_fullscreen_;
  bool is_in_transition_ = false;

  // Starting in 10.11, calling -[NSWindow setStyleMask:] can actually invoke
  // the fullscreen transitions we want to fake. So, when set, this will include
  // NSWindowStyleMaskFullScreen in the swizzled styleMask so that client code
  // can read it.
  bool style_as_fullscreen_ = false;
};

ScopedFakeNSWindowFullscreen::ScopedFakeNSWindowFullscreen() {
  DCHECK(!g_fake_fullscreen_impl);
  impl_ = std::make_unique<Impl>();
  g_fake_fullscreen_impl = impl_.get();
  g_instance_count += 1;
  BaseWindow::SetFullscreenFakedForTesting(g_instance_count > 0);
}

ScopedFakeNSWindowFullscreen::~ScopedFakeNSWindowFullscreen() {
  g_fake_fullscreen_impl = nullptr;
  g_instance_count -= 1;
  BaseWindow::SetFullscreenFakedForTesting(g_instance_count > 0);
}

void ScopedFakeNSWindowFullscreen::FinishTransition() {
  if (impl_->is_in_transition())
    base::RunLoop().RunUntilIdle();

  DCHECK(!impl_->is_in_transition());
}

}  // namespace ui::test

@implementation ToggleFullscreenDonorForWindow

- (void)toggleFullScreen:(id)sender {
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(self);
  g_fake_fullscreen_impl->ToggleFullscreenForWindow(window);
}

- (NSUInteger)styleMask {
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(self);
  return g_fake_fullscreen_impl->StyleMaskForWindow(window);
}

- (void)setStyleMask:(NSUInteger)newMask {
  // Permit the non-fullscreen bits of the style mask to be changed while
  // currently fullscreen, but don't let AppKit see any fullscreen bits.
  NSUInteger currentMask = [self styleMask];
  if ((newMask ^ currentMask) & NSWindowStyleMaskFullScreen) {
    // Since 10.11, OSX triggers fullscreen transitions via setStyleMask, but
    // the faker doesn't attempt to fake them yet.
    NOTREACHED()
        << "Can't set NSWindowStyleMaskFullScreen while faking fullscreen.";
  }
  newMask &= ~NSWindowStyleMaskFullScreen;
  g_fake_fullscreen_impl->OriginalSetStyleMask(self, _cmd, newMask);
}

@end
