// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace ui {

namespace {

// This implementation uses the obsolete CGWindowListCreateImage API.
// TODO(https://crbug.com/1464728): When there is a ScreenCaptureKit API that
// allows window self-capture without menu chip and without TCC requirements,
// switch to that API.
void GrabViewSnapshotImpl(gfx::NativeView native_view,
                          const gfx::Rect& snapshot_bounds,
                          gfx::Image* image) {
  NSView* view = native_view.GetNativeNSView();
  NSWindow* window = view.window;
  NSScreen* screen = NSScreen.screens.firstObject;
  gfx::Rect screen_bounds = gfx::Rect(NSRectToCGRect(screen.frame));

  // Get the view bounds relative to the screen
  NSRect frame = [view convertRect:view.bounds toView:nil];
  frame = [window convertRectToScreen:frame];

  gfx::Rect view_bounds = gfx::Rect(NSRectToCGRect(frame));

  // Flip window coordinates based on the primary screen.
  view_bounds.set_y(
      screen_bounds.height() - view_bounds.y() - view_bounds.height());

  // Convert snapshot bounds relative to window into bounds relative to
  // screen.
  gfx::Rect screen_snapshot_bounds = snapshot_bounds;
  screen_snapshot_bounds.Offset(view_bounds.OffsetFromOrigin());

  DCHECK_LE(screen_snapshot_bounds.right(), view_bounds.right());
  DCHECK_LE(screen_snapshot_bounds.bottom(), view_bounds.bottom());

  base::apple::ScopedCFTypeRef<CGImageRef> windowSnapshot(
      CGWindowListCreateImage(
          screen_snapshot_bounds.ToCGRect(), kCGWindowListOptionIncludingWindow,
          window.windowNumber, kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot.get()) <= 0) {
    return;
  }

  *image = gfx::Image([[NSImage alloc] initWithCGImage:windowSnapshot.get()
                                                  size:NSZeroSize]);
  return;
}

}  // namespace

void GrabWindowSnapshotAsync(gfx::NativeWindow native_window,
                             const gfx::Rect& source_rect,
                             GrabSnapshotImageCallback callback) {
  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  NSView* view = native_window.GetNativeNSWindow().contentView.superview;

  gfx::Image image;
  GrabViewSnapshotImpl(view, source_rect, &image);
  std::move(callback).Run(image);
}

void GrabViewSnapshotAsync(gfx::NativeView view,
                           const gfx::Rect& source_rect,
                           GrabSnapshotImageCallback callback) {
  gfx::Image image;
  GrabViewSnapshotImpl(view, source_rect, &image);
  std::move(callback).Run(image);
}

}  // namespace ui
