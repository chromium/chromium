// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/views/accessibility/tree/widget_view_ax_cache.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace views {

WidgetAXManager::WidgetAXManager(Widget* widget)
    : widget_(widget),
      ax_tree_id_(ui::AXTreeID::CreateNewAXTreeID()),
      cache_(std::make_unique<WidgetViewAXCache>()) {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled())
      << "WidgetAXManager should only be created when the "
         "accessibility tree feature is enabled.";

  ui::AXPlatform::GetInstance().AddModeObserver(this);

  if (ui::AXPlatform::GetInstance().GetMode() == ui::AXMode::kNativeAPIs) {
    Enable();
  }
}

WidgetAXManager::~WidgetAXManager() {
  ui::AXPlatform::GetInstance().RemoveModeObserver(this);
}

void WidgetAXManager::Enable() {
  is_enabled_ = true;
  tree_source_ = std::make_unique<ViewAccessibilityAXTreeSource>(
      widget_->GetRootView()->GetViewAccessibility().GetUniqueId(), ax_tree_id_,
      cache_.get());
  tree_serializer_ =
      std::make_unique<ViewAccessibilityAXTreeSerializer>(tree_source_.get());
}

void WidgetAXManager::OnEvent(ViewAccessibility& view_ax,
                              ax::mojom::Event event_type) {
  if (!is_enabled_) {
    return;
  }

  pending_events_.push_back({view_ax.GetUniqueId(), event_type});
  pending_data_updates_.insert(view_ax.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnDataChanged(ViewAccessibility& view_ax) {
  if (!is_enabled_) {
    return;
  }

  pending_data_updates_.insert(view_ax.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnChildAdded(WidgetAXManager* child_manager) {
  CHECK(child_manager);
  child_manager->parent_ax_tree_id_ = ax_tree_id_;
}

void WidgetAXManager::OnChildRemoved(WidgetAXManager* child_manager) {
  CHECK(child_manager);
  child_manager->parent_ax_tree_id_ = ui::AXTreeID();
}

void WidgetAXManager::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kNativeAPIs)) {
    Enable();
  }
}

void WidgetAXManager::SchedulePendingUpdate() {
  if (processing_update_posted_ || !is_enabled_) {
    return;
  }

  processing_update_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WidgetAXManager::SendPendingUpdate,
                                weak_factory_.GetWeakPtr()));
}

void WidgetAXManager::SendPendingUpdate() {
  processing_update_posted_ = false;
  if (!is_enabled_) {
    return;
  }

  // TODO(accessibility): Implement the serialization.
  pending_events_.clear();
  pending_data_updates_.clear();
}

}  // namespace views
