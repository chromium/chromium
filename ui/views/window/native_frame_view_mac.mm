// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/base/metadata/metadata_impl_macros.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/widget.h"

namespace views {

NativeFrameViewMac::NativeFrameViewMac(Widget* widget)
    : NativeFrameView(widget) {}

NativeFrameViewMac::~NativeFrameViewMac() = default;

gfx::Rect NativeFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  NSWindow* ns_window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  gfx::Rect window_bounds = gfx::ScreenRectFromNSRect([ns_window
      frameRectForContentRect:gfx::ScreenRectToNSRect(client_bounds)]);
  // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
  // empty size.
  if (window_bounds.IsEmpty())
    window_bounds.set_size(gfx::Size(1, 1));
  return window_bounds;
}

BEGIN_METADATA(NativeFrameViewMac)
END_METADATA

}  // namespace views
