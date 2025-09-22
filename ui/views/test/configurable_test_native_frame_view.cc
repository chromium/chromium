// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/configurable_test_native_frame_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

namespace views::test {

ConfigurableTestNativeFrameView::ConfigurableTestNativeFrameView(Widget* widget)
    : NativeFrameView(widget) {}
ConfigurableTestNativeFrameView::~ConfigurableTestNativeFrameView() = default;

gfx::Size ConfigurableTestNativeFrameView::GetMinimumSize() const {
  if (minimum_size_) {
    return minimum_size_.value();
  }

  return NativeFrameView::GetMinimumSize();
}

gfx::Rect ConfigurableTestNativeFrameView::GetBoundsForClientView() const {
  gfx::Rect client_view_bounds = NativeFrameView::GetBoundsForClientView();

  if (client_view_margin_) {
    client_view_bounds.set_width(client_view_bounds.width() -
                                 client_view_margin_->width());
    client_view_bounds.set_height(client_view_bounds.height() -
                                  client_view_margin_->height());
  }

  return client_view_bounds;
}

gfx::Rect ConfigurableTestNativeFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  if (client_view_margin_) {
    // If a margin is set, adjust the window bounds to account for the custom
    // margin. We do this to answer the question:
    // "How large is the window for a client with size `client_bounds?`"
    gfx::Rect window_bounds = client_bounds;
    window_bounds.set_width(window_bounds.width() +
                            client_view_margin_->width());
    window_bounds.set_height(window_bounds.height() +
                             client_view_margin_->height());
    return window_bounds;
  }

  return NativeFrameView::GetWindowBoundsForClientBounds(client_bounds);
}

int ConfigurableTestNativeFrameView::NonClientHitTest(const gfx::Point& point) {
  if (hit_test_result_) {
    return hit_test_result_.value();
  }

  return NativeFrameView::NonClientHitTest(point);
}

void ConfigurableTestNativeFrameView::Layout(PassKey) {
  if (GetWidget() && GetWidget()->IsFullscreen()) {
    fullscreen_layout_caled_ = true;
  }
}

BEGIN_METADATA(ConfigurableTestNativeFrameView);
END_METADATA
}  // namespace views::test
