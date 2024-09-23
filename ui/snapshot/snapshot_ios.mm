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

void GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& source_rect,
                      GrabSnapshotImageCallback callback) {
  gfx::Image image;

  UIView* source_view = view.Get();
  if (source_view) {
    UIImage* snapshot = GetViewSnapshot(source_view, source_rect.ToCGRect());
    if (snapshot) {
      image = gfx::Image(snapshot);
    }
  }

  std::move(callback).Run(image);
}

void GrabWindowSnapshot(gfx::NativeWindow window,
                        const gfx::Rect& source_rect,
                        GrabSnapshotImageCallback callback) {
  gfx::Image image;

  UIWindow* source_window = window.Get();
  if (source_window) {
    UIImage* snapshot = GetViewSnapshot(source_window.rootViewController.view,
                                        source_rect.ToCGRect());
    if (snapshot) {
      image = gfx::Image(snapshot);
    }
  }

  std::move(callback).Run(image);
}

}  // namespace ui
