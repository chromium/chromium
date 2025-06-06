// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/configurable_test_non_client_frame_view.h"

#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace views::test {

ConfigurableTestNonClientFrameView::ConfigurableTestNonClientFrameView() =
    default;
ConfigurableTestNonClientFrameView::~ConfigurableTestNonClientFrameView() =
    default;

gfx::Rect ConfigurableTestNonClientFrameView::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect ConfigurableTestNonClientFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int ConfigurableTestNonClientFrameView::NonClientHitTest(
    const gfx::Point& point) {
  if (hit_test_callback_) {
    return hit_test_callback_.Run(point);
  }

  return HTNOWHERE;
}

void ConfigurableTestNonClientFrameView::GetWindowMask(const gfx::Size& size,
                                                       SkPath* window_mask) {
  if (window_mask_callback_) {
    window_mask_callback_.Run(size, window_mask);
  }
}

void ConfigurableTestNonClientFrameView::ResetWindowControls() {}
void ConfigurableTestNonClientFrameView::UpdateWindowIcon() {}
void ConfigurableTestNonClientFrameView::UpdateWindowTitle() {}
void ConfigurableTestNonClientFrameView::SizeConstraintsChanged() {}

BEGIN_METADATA(ConfigurableTestNonClientFrameView)
END_METADATA

}  // namespace views::test
