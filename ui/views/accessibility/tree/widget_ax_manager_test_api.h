// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_

#include "ui/views/accessibility/tree/widget_ax_manager.h"

namespace views {

class WidgetAXManagerTestApi {
 public:
  explicit WidgetAXManagerTestApi(WidgetAXManager* manager)
      : manager_(manager) {}

  const auto& pending_events() const { return manager_->pending_events_; }
  const auto& pending_data_updates() const {
    return manager_->pending_data_updates_;
  }
  bool processing_update_posted() const {
    return manager_->processing_update_posted_;
  }

  const ui::AXTreeID& ax_tree_id() const { return manager_->ax_tree_id_; }
  const ui::AXTreeID& parent_ax_tree_id() const {
    return manager_->parent_ax_tree_id_;
  }

  void TearDown() { manager_ = nullptr; }

 private:
  raw_ptr<WidgetAXManager> manager_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_
