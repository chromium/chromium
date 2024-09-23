// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_layout_manager.h"

namespace views::test {

TestLayoutManager::TestLayoutManager() = default;

TestLayoutManager::~TestLayoutManager() = default;

views::ProposedLayout TestLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  if (!size_bounds.height().is_bounded()) {
    layout.host_size = preferred_size_;
    if (size_bounds.width().is_bounded()) {
      layout.host_size.set_height(preferred_height_for_width_);
    }
  } else {
    layout.host_size =
        gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  }
  return layout;
}

void TestLayoutManager::OnLayoutChanged() {
  views::LayoutManagerBase::OnLayoutChanged();
  ++invalidate_count_;
}

}  // namespace views::test
