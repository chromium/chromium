// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_

#include <optional>
#include <vector>

#include "base/run_loop.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"
#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

namespace views {

class WidgetAXManagerTestApi {
 public:
  explicit WidgetAXManagerTestApi(WidgetAXManager* manager);
  ~WidgetAXManagerTestApi();

  void Enable();

  const std::vector<WidgetAXManager::Event>& pending_events() const;
  const absl::flat_hash_set<ui::AXNodeID>& pending_data_updates() const;
  bool processing_update_posted() const;

  const ui::AXTreeID& ax_tree_id() const;
  const ui::AXTreeID& parent_ax_tree_id() const;
  WidgetViewAXCache* cache() const;
  ui::BrowserAccessibilityManager* ax_tree_manager() const;
  const ui::AXUpdatesAndEvents& last_serialization() const;
  bool has_last_serialization() const;

  void TearDown();

  // Blocks until the next signal from SendPendingUpdate() is received,
  // signifying either that something got serialized or that we won't
  // serialize anything.
  void WaitForNextSerialization();

 private:
  void OnUpdatesAndEvents(const std::optional<ui::AXUpdatesAndEvents>& opt);

  raw_ptr<WidgetAXManager> manager_;
  std::optional<ui::AXUpdatesAndEvents> last_serialization_;

  raw_ptr<base::RunLoop> waiting_run_loop_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_TEST_API_H_
