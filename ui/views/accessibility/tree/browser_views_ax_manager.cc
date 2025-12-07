// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/browser_views_ax_manager.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/accessibility_alert_window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace views {

BrowserViewsAXManager* g_instance = nullptr;

BrowserViewsAXManager::LifetimeHandle::LifetimeHandle(
    BrowserViewsAXManagerPassKey) {
  instance_ = std::make_unique<BrowserViewsAXManager>(LifetimeHandlePassKey());
}

BrowserViewsAXManager::LifetimeHandle::~LifetimeHandle() = default;

BrowserViewsAXManager::BrowserViewsAXManager(LifetimeHandlePassKey) {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled());
  CHECK(!g_instance);
  g_instance = this;
  ui::AXPlatform::GetInstance().AddModeObserver(this);

  if (ui::AXPlatform::GetInstance().GetMode() == ui::AXMode::kNativeAPIs) {
    Enable();
  }
}

BrowserViewsAXManager::~BrowserViewsAXManager() {
  DCHECK_EQ(g_instance, this);
  ui::AXPlatform::GetInstance().RemoveModeObserver(this);
  g_instance = nullptr;
}

std::unique_ptr<BrowserViewsAXManager::LifetimeHandle>
BrowserViewsAXManager::Create() {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled());
  CHECK(!g_instance);
  return std::make_unique<LifetimeHandle>(
      LifetimeHandle::BrowserViewsAXManagerPassKey());
}

BrowserViewsAXManager* BrowserViewsAXManager::GetInstance() {
  CHECK(g_instance);
  return g_instance;
}

ui::AXPlatformNodeId BrowserViewsAXManager::GetOrCreateAXNodeUniqueId(
    ui::AXNodeID ax_node_id) {
  auto iter = ax_unique_ids_.lower_bound(ax_node_id);
  if (iter == ax_unique_ids_.end() || iter->first != ax_node_id) {
    iter =
        ax_unique_ids_.emplace_hint(iter, ax_node_id, ui::AXUniqueId::Create());
  }
  return iter->second;
}

void BrowserViewsAXManager::OnAXNodeDeleted(ui::AXNodeID ax_node_id) {
  ax_unique_ids_.erase(ax_node_id);
}

void BrowserViewsAXManager::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  NOTIMPLEMENTED();
}

bool BrowserViewsAXManager::AccessibilityViewHasFocus() {
  NOTIMPLEMENTED();
  return false;
}

void BrowserViewsAXManager::AccessibilityViewSetFocus() {
  NOTIMPLEMENTED();
}

gfx::Rect BrowserViewsAXManager::AccessibilityGetViewBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

float BrowserViewsAXManager::AccessibilityGetDeviceScaleFactor() {
  NOTIMPLEMENTED();
  return 1.0f;
}

void BrowserViewsAXManager::UnrecoverableAccessibilityError() {
  NOTIMPLEMENTED();
}

gfx::AcceleratedWidget
BrowserViewsAXManager::AccessibilityGetAcceleratedWidget() {
  NOTIMPLEMENTED();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
BrowserViewsAXManager::AccessibilityGetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeViewAccessible
BrowserViewsAXManager::AccessibilityGetNativeViewAccessibleForWindow() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::NativeViewAccessible
BrowserViewsAXManager::GetRootViewNativeViewAccessible() {
  if (!is_enabled()) {
    return nullptr;
  }

  ui::BrowserAccessibility* root =
      ax_tree_manager_->GetBrowserAccessibilityRoot();
  if (!root) {
    return nullptr;
  }
  return root->GetNativeViewAccessible();
}

void BrowserViewsAXManager::AccessibilityHitTest(
    const gfx::Point& point_in_frame_pixels,
    const ax::mojom::Event& opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            ui::AXNodeID hit_node_id)> opt_callback) {
  NOTIMPLEMENTED();
}

gfx::NativeWindow BrowserViewsAXManager::GetTopLevelNativeWindow() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BrowserViewsAXManager::CanFireAccessibilityEvents() const {
  return is_enabled();
}

bool BrowserViewsAXManager::AccessibilityIsRootFrame() const {
  return false;
}

bool BrowserViewsAXManager::ShouldSuppressAXLoadComplete() {
  NOTIMPLEMENTED();
  return false;
}

content::WebContentsAccessibility*
BrowserViewsAXManager::AccessibilityGetWebContentsAccessibility() {
  // This will never be a web content tree.
  return nullptr;
}

bool BrowserViewsAXManager::AccessibilityIsWebContentSource() {
  return false;
}

void BrowserViewsAXManager::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kNativeAPIs)) {
    Enable();
  }
}

void BrowserViewsAXManager::Reset(bool reset_serializer) {
  ViewsAXManager::Reset(reset_serializer);
  ax_tree_manager_.reset(ui::BrowserAccessibilityManager::Create(*this, this));
}

}  // namespace views
