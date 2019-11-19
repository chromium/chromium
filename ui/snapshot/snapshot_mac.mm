// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView native_view,
                      const gfx::Rect& snapshot_bounds,
                      gfx::Image* image) {
  NSView* view = native_view.GetNativeNSView();
  NSWindow* window = [view window];
  NSScreen* screen = [[NSScreen screens] firstObject];
  gfx::Rect screen_bounds = gfx::Rect(NSRectToCGRect([screen frame]));


  // Get the view bounds relative to the screen
  NSRect frame = [view convertRect:[view bounds] toView:nil];
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

  base::ScopedCFTypeRef<CGImageRef> windowSnapshot(
      CGWindowListCreateImage(screen_snapshot_bounds.ToCGRect(),
                              kCGWindowListOptionIncludingWindow,
                              [window windowNumber],
                              kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot) <= 0)
    return false;

  *image =
      gfx::Image([[[NSImage alloc] initWithCGImage:windowSnapshot
                                              size:NSZeroSize] autorelease]);
  return true;
}

bool GrabWindowSnapshot(gfx::NativeWindow native_window,
                        const gfx::Rect& snapshot_bounds,
                        gfx::Image* image) {
  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  NSWindow* window = native_window.GetNativeNSWindow();
  return GrabViewSnapshot([[window contentView] superview], snapshot_bounds,
                          image);
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const gfx::Size& target_size,
    GrabWindowSnapshotAsyncCallback callback) {
  std::move(callback).Run(gfx::Image());
}

void GrabViewSnapshotAsync(gfx::NativeView view,
                           const gfx::Rect& source_rect,
                           GrabWindowSnapshotAsyncCallback callback) {
  std::move(callback).Run(gfx::Image());
}

void GrabWindowSnapshotAsync(gfx::NativeWindow native_window,
                             const gfx::Rect& source_rect,
                             GrabWindowSnapshotAsyncCallback callback) {
  NSWindow* window = native_window.GetNativeNSWindow();
  return GrabViewSnapshotAsync([[window contentView] superview], source_rect,
                               std::move(callback));
}

}  // namespace ui
