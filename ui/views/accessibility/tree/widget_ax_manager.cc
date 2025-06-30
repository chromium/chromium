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

void WidgetAXManager::AccessibilityPerformAction(const ui::AXActionData& data) {
  tree_source_->HandleAccessibleAction(data);
}

bool WidgetAXManager::AccessibilityViewHasFocus() {
  return widget_ && widget_->IsActive();
}

void WidgetAXManager::AccessibilityViewSetFocus() {
  if (!widget_ || widget_->IsActive()) {
    return;
  }
  widget_->Activate();
}

gfx::Rect WidgetAXManager::AccessibilityGetViewBounds() {
  if (!widget_) {
    return gfx::Rect();
  }
  return widget_->GetWindowBoundsInScreen();
}

float WidgetAXManager::AccessibilityGetDeviceScaleFactor() {
  // TODO(crbug.com/40672441): Confirm that the DSF is always 1.0f once we
  // serialize the views and can test it manually.
  return 1.0f;
}

void WidgetAXManager::UnrecoverableAccessibilityError() {
  // TODO(accessibility): Implement.
}

gfx::AcceleratedWidget WidgetAXManager::AccessibilityGetAcceleratedWidget() {
  // TODO(accessibility): Implement.
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
WidgetAXManager::AccessibilityGetNativeViewAccessible() {
  // TODO(accessibility): Implement.
  return gfx::NativeViewAccessible();
}

gfx::NativeViewAccessible
WidgetAXManager::AccessibilityGetNativeViewAccessibleForWindow() {
  // TODO(accessibility): Implement.
  return gfx::NativeViewAccessible();
}

void WidgetAXManager::AccessibilityHitTest(
    const gfx::Point& point_in_view_pixels,
    const ax::mojom::Event& opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            ui::AXNodeID hit_node_id)> opt_callback) {
  // TODO(accessibility): Implement.
}

gfx::NativeWindow WidgetAXManager::GetTopLevelNativeWindow() {
  // TODO(accessibility): Implement.
  return gfx::NativeWindow();
}

bool WidgetAXManager::CanFireAccessibilityEvents() const {
  // TODO(accessibility): Implement.
  return false;
}

bool WidgetAXManager::AccessibilityIsRootFrame() const {
  // TODO(accessibility): Implement.
  return false;
}

bool WidgetAXManager::ShouldSuppressAXLoadComplete() {
  // TODO(accessibility): Implement.
  return false;
}

content::WebContentsAccessibility*
WidgetAXManager::AccessibilityGetWebContentsAccessibility() {
  return nullptr;
}

bool WidgetAXManager::AccessibilityIsWebContentSource() {
  return false;
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
