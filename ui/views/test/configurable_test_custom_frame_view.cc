// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/configurable_test_custom_frame_view.h"

#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace views::test {

ConfigurableTestCustomFrameView::ConfigurableTestCustomFrameView() = default;
ConfigurableTestCustomFrameView::~ConfigurableTestCustomFrameView() = default;

gfx::Rect ConfigurableTestCustomFrameView::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect ConfigurableTestCustomFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int ConfigurableTestCustomFrameView::NonClientHitTest(const gfx::Point& point) {
  if (hit_test_callback_) {
    return hit_test_callback_.Run(point);
  }

  return HTNOWHERE;
}

void ConfigurableTestCustomFrameView::GetWindowMask(const gfx::Size& size,
                                                    SkPath* window_mask) {
  if (window_mask_callback_) {
    window_mask_callback_.Run(size, window_mask);
  }
}

void ConfigurableTestCustomFrameView::ResetWindowControls() {}
void ConfigurableTestCustomFrameView::UpdateWindowIcon() {}
void ConfigurableTestCustomFrameView::UpdateWindowTitle() {}
void ConfigurableTestCustomFrameView::SizeConstraintsChanged() {}

BEGIN_METADATA(ConfigurableTestCustomFrameView)
END_METADATA

}  // namespace views::test
