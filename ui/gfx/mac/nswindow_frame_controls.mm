// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/mac/nswindow_frame_controls.h"

#import <AppKit/AppKit.h>

#include "ui/gfx/geometry/size.h"

namespace gfx {

void SetNSWindowCanFullscreen(NSWindow* window, bool allow_fullscreen) {
  NSWindowCollectionBehavior behavior = window.collectionBehavior;
  if (behavior & NSWindowCollectionBehaviorFullScreenAuxiliary) {
    return;
  }
  if (allow_fullscreen) {
    behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
  } else {
    behavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
  }
  window.collectionBehavior = behavior;
}

void SetNSWindowVisibleOnAllWorkspaces(NSWindow* window, bool always_visible) {
  NSWindowCollectionBehavior behavior = window.collectionBehavior;
  if (always_visible) {
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces;
  } else {
    behavior &= ~NSWindowCollectionBehaviorCanJoinAllSpaces;
  }
  window.collectionBehavior = behavior;
}

void ApplyNSWindowSizeConstraints(NSWindow* window,
                                  const gfx::Size& min_size,
                                  const gfx::Size& max_size,
                                  bool can_resize,
                                  bool can_fullscreen) {
  const int kUnboundedSize = 0;

  CGFloat max_width =
      max_size.width() == kUnboundedSize ? CGFLOAT_MAX : max_size.width();
  CGFloat max_height =
      max_size.height() == kUnboundedSize ? CGFLOAT_MAX : max_size.height();

  window.contentMinSize = min_size.ToCGSize();
  window.contentMaxSize = CGSizeMake(max_width, max_height);

  NSUInteger style_mask = window.styleMask;
  if (can_resize) {
    style_mask |= NSWindowStyleMaskResizable;
  } else {
    style_mask &= ~NSWindowStyleMaskResizable;
  }
  window.styleMask = style_mask;

  SetNSWindowCanFullscreen(window, can_fullscreen);

  [window standardWindowButton:NSWindowZoomButton].enabled = can_fullscreen;
}

}  // namespace gfx
