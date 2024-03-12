// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_LAYOUT_MANAGER_H_
#define UI_VIEWS_TEST_TEST_LAYOUT_MANAGER_H_

#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager_base.h"

namespace views::test {

// A stub layout manager that returns a specific preferred size and height for
// width.
class TestLayoutManager : public LayoutManagerBase {
 public:
  TestLayoutManager();

  TestLayoutManager(const TestLayoutManager&) = delete;
  TestLayoutManager& operator=(const TestLayoutManager&) = delete;

  ~TestLayoutManager() override;

  void SetPreferredSize(const gfx::Size& size) { preferred_size_ = size; }

  void set_preferred_height_for_width(int height) {
    preferred_height_for_width_ = height;
  }

  int invalidate_count() const { return invalidate_count_; }

 protected:
  // LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  void OnLayoutChanged() override;

 private:
  // The return value of GetPreferredSize();
  gfx::Size preferred_size_;

  // The return value for GetPreferredHeightForWidth().
  int preferred_height_for_width_ = 0;

  // The number of calls to InvalidateLayout().
  int invalidate_count_ = 0;
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_TEST_LAYOUT_MANAGER_H_
