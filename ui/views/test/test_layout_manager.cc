// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_layout_manager.h"

namespace views::test {

TestLayoutManager::TestLayoutManager() = default;

TestLayoutManager::~TestLayoutManager() = default;

void TestLayoutManager::Layout(View* host) {}

gfx::Size TestLayoutManager::GetPreferredSize(const View* host) const {
  return preferred_size_;
}

int TestLayoutManager::GetPreferredHeightForWidth(const View* host,
                                                  int width) const {
  return preferred_height_for_width_;
}

void TestLayoutManager::InvalidateLayout() {
  ++invalidate_count_;
}

}  // namespace views::test
