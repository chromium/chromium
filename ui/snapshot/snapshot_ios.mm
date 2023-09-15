// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <UIKit/UIKit.h>

#include "base/functional/callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace ui {

namespace {
UIImage* GetViewSnapshot(UIView* view, CGRect bounds) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = [[UIScreen mainScreen] scale];
  format.opaque = NO;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:bounds.size format:format];
  UIImage* snapshot =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [view drawViewHierarchyInRect:bounds afterScreenUpdates:YES];
      }];
  return snapshot;
}
}  // namespace

bool GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& snapshot_bounds,
                      gfx::Image* image) {
  UIView* source_view = view.Get();
  if (source_view == nil) {
    return false;
  }

  UIImage* snapshot = GetViewSnapshot(source_view, snapshot_bounds.ToCGRect());
  if (snapshot) {
    *image = gfx::Image(snapshot);
    return true;
  }
  return false;
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& snapshot_bounds,
                        gfx::Image* image) {
  UIWindow* source_window = window.Get();
  if (source_window == nil) {
    return false;
  }

  UIImage* snapshot = GetViewSnapshot(source_window.rootViewController.view,
                                      snapshot_bounds.ToCGRect());
  if (snapshot) {
    *image = gfx::Image(snapshot);
    return true;
  }
  return false;
}

void GrabWindowSnapshotAndScaleAsync(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    const gfx::Size& target_size,
    GrabWindowSnapshotAsyncCallback callback) {
  gfx::Image image;
  if (GrabWindowSnapshot(window, snapshot_bounds, &image)) {
    gfx::Image resized_image = gfx::ResizedImage(image, target_size);
    std::move(callback).Run(resized_image);
    return;
  }
  std::move(callback).Run(image);
}

void GrabViewSnapshotAsync(gfx::NativeView view,
                           const gfx::Rect& source_rect,
                           GrabWindowSnapshotAsyncCallback callback) {
  gfx::Image image;
  GrabViewSnapshot(view, source_rect, &image);
  std::move(callback).Run(image);
}

void GrabWindowSnapshotAsync(gfx::NativeWindow window,
                             const gfx::Rect& source_rect,
                             GrabWindowSnapshotAsyncCallback callback) {
  gfx::Image image;
  GrabWindowSnapshot(window, source_rect, &image);
  std::move(callback).Run(image);
}

}  // namespace ui
