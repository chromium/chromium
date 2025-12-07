// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view_mac.h"

#import <Cocoa/Cocoa.h>

#include <optional>

#include "ui/base/metadata/metadata_impl_macros.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

NativeFrameViewMac::NativeFrameViewMac(Widget* widget,
                                       NativeFrameViewMacClient* client)
    : NativeFrameView(widget), client_(client) {}

NativeFrameViewMac::~NativeFrameViewMac() = default;

gfx::Rect NativeFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  NSWindow* ns_window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  gfx::Rect window_bounds = gfx::ScreenRectFromNSRect([ns_window
      frameRectForContentRect:gfx::ScreenRectToNSRect(client_bounds)]);
  // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
  // empty size.
  if (window_bounds.IsEmpty()) {
    window_bounds.set_size(gfx::Size(1, 1));
  }
  return window_bounds;
}

int NativeFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  if (client_) {
    if (std::optional<int> result = client_->NonClientHitTest(point)) {
      return result.value();
    }
  }

  return NativeFrameView::NonClientHitTest(point);
}

BEGIN_METADATA(NativeFrameViewMac)
END_METADATA

}  // namespace views
