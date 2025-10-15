// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"

namespace views {

WidgetAXManagerTestApi::WidgetAXManagerTestApi(WidgetAXManager* manager)
    : manager_(manager), waiting_run_loop_(nullptr) {
  manager_->SetUpdatesAndEventsCallbackForTesting(base::BindRepeating(
      &WidgetAXManagerTestApi::OnUpdatesAndEvents, base::Unretained(this)));
}

WidgetAXManagerTestApi::~WidgetAXManagerTestApi() = default;

void WidgetAXManagerTestApi::Enable() {
  manager_->Enable();
}

const std::vector<WidgetAXManager::Event>&
WidgetAXManagerTestApi::pending_events() const {
  return manager_->pending_events_;
}

const absl::flat_hash_set<ui::AXNodeID>&
WidgetAXManagerTestApi::pending_data_updates() const {
  return manager_->pending_data_updates_;
}

bool WidgetAXManagerTestApi::processing_update_posted() const {
  return manager_->processing_update_posted_;
}

const ui::AXTreeID& WidgetAXManagerTestApi::ax_tree_id() const {
  return manager_->ax_tree_id_;
}

const ui::AXTreeID& WidgetAXManagerTestApi::parent_ax_tree_id() const {
  return manager_->parent_ax_tree_id_;
}

WidgetViewAXCache* WidgetAXManagerTestApi::cache() const {
  return manager_->cache_.get();
}

ui::BrowserAccessibilityManager* WidgetAXManagerTestApi::ax_tree_manager()
    const {
  return manager_->ax_tree_manager_.get();
}

const ui::AXUpdatesAndEvents& WidgetAXManagerTestApi::last_serialization()
    const {
  return last_serialization_.value();
}

bool WidgetAXManagerTestApi::has_last_serialization() const {
  return last_serialization_.has_value();
}

void WidgetAXManagerTestApi::TearDown() {
  manager_ = nullptr;
}

void WidgetAXManagerTestApi::WaitForNextSerialization() {
  CHECK(manager_);
  base::RunLoop run_loop;
  CHECK(!waiting_run_loop_);
  waiting_run_loop_ = &run_loop;
  run_loop.Run();
}

void WidgetAXManagerTestApi::OnUpdatesAndEvents(
    const std::optional<ui::AXUpdatesAndEvents>& opt) {
  if (opt.has_value()) {
    last_serialization_.emplace();
    last_serialization_->ax_tree_id = opt->ax_tree_id;
    last_serialization_->updates = opt->updates;
    last_serialization_->events = opt->events;
  } else {
    last_serialization_.reset();
  }

  if (waiting_run_loop_) {
    waiting_run_loop_->Quit();
    waiting_run_loop_ = nullptr;
  }
}

}  // namespace views
