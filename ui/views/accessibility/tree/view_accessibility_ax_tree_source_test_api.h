// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_TEST_API_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_TEST_API_H_

#include "ui/views/accessibility/tree/view_accessibility_ax_tree_source.h"
#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

namespace views {

class ViewAccessibilityAXTreeSourceTestApi {
 public:
  explicit ViewAccessibilityAXTreeSourceTestApi(
      ViewAccessibilityAXTreeSource* source)
      : source_(source) {}

  ui::AXNodeID root_id() const { return source_->root_id_; }
  const ui::AXTreeID& tree_id() const { return source_->tree_id_; }
  WidgetViewAXCache& cache() const { return *source_->cache_; }

  void TearDown() { source_ = nullptr; }

 private:
  raw_ptr<ViewAccessibilityAXTreeSource> source_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_TEST_API_H_
