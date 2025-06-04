// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/configurable_test_frame_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/native_frame_view.h"

namespace views::test {

ConfigurableTestFrameView::ConfigurableTestFrameView(Widget* frame)
    : NativeFrameView(frame) {}
ConfigurableTestFrameView::~ConfigurableTestFrameView() = default;

gfx::Size ConfigurableTestFrameView::GetMinimumSize() const {
  if (minimum_size_) {
    return minimum_size_.value();
  }

  return NativeFrameView::GetMinimumSize();
}

int ConfigurableTestFrameView::NonClientHitTest(const gfx::Point& point) {
  if (hit_test_result_) {
    return hit_test_result_.value();
  }

  return NativeFrameView::NonClientHitTest(point);
}

BEGIN_METADATA(ConfigurableTestFrameView);
END_METADATA
}  // namespace views::test
