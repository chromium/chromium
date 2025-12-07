// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_impl.h"

#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider_impl.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {
namespace {

// Error allowed for each edge when converting from gfx::RectF to gfx::Rect.
constexpr float kRectConversionError = 0.5;

std::optional<ax::mojom::Action> ConvertAction(
    fuchsia_accessibility_semantics::Action fuchsia_action) {
  switch (fuchsia_action) {
    case fuchsia_accessibility_semantics::Action::kDefault:
      return ax::mojom::Action::kDoDefault;
    case fuchsia_accessibility_semantics::Action::kDecrement:
      return ax::mojom::Action::kDecrement;
    case fuchsia_accessibility_semantics::Action::kIncrement:
      return ax::mojom::Action::kIncrement;
    case fuchsia_accessibility_semantics::Action::kShowOnScreen:
      return ax::mojom::Action::kScrollToMakeVisible;
    case fuchsia_accessibility_semantics::Action::kSecondary:
      LOG(WARNING) << "SECONDARY action not supported";
      return {};
    case fuchsia_accessibility_semantics::Action::kSetFocus:
      return ax::mojom::Action::kFocus;
    case fuchsia_accessibility_semantics::Action::kSetValue:
      return ax::mojom::Action::kSetValue;
    default:
      LOG(WARNING)
          << "Unknown fuchsia_accessibility_semantics::Action with value "
          << static_cast<int>(fuchsia_action);
      return {};
  }
}

}  // namespace

AccessibilityBridgeFuchsiaImpl::AccessibilityBridgeFuchsiaImpl(
    aura::Window* window,
    fuchsia_ui_views::ViewRef view_ref,
    base::RepeatingCallback<void(bool)> on_semantics_enabled,
    OnConnectionClosedCallback on_connection_closed,
    inspect::Node inspect_node)
    : root_window_(window),
      on_semantics_enabled_(std::move(on_semantics_enabled)),
      on_connection_closed_(std::move(on_connection_closed)),
      inspect_node_(std::move(inspect_node)) {
  semantic_provider_ = std::make_unique<AXFuchsiaSemanticProviderImpl>(
      std::move(view_ref), this);

  AccessibilityBridgeFuchsiaRegistry* registry =
      AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(registry);
  if (root_window_)
    registry->RegisterAccessibilityBridge(root_window_, this);
}

AccessibilityBridgeFuchsiaImpl::~AccessibilityBridgeFuchsiaImpl() {
  AccessibilityBridgeFuchsiaRegistry* registry =
      AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(registry);
  if (root_window_)
    registry->UnregisterAccessibilityBridge(root_window_);
}

uint32_t AccessibilityBridgeFuchsiaImpl::MaybeToFuchsiaRootID(
    uint32_t node_id) {
  // If |node_id| refers to the root, then map to fuchsia ID 0. Otherwise,
  // use |node_id| directly.
  if (root_node_id_ && node_id == *root_node_id_)
    return AXFuchsiaSemanticProvider::kFuchsiaRootNodeId;

  return node_id;
}

void AccessibilityBridgeFuchsiaImpl::UpdateNode(
    fuchsia_accessibility_semantics::Node node) {
  DCHECK(node.node_id().has_value());

  node.node_id(MaybeToFuchsiaRootID(node.node_id().value()));

  if (node.container_id()) {
    node.container_id(MaybeToFuchsiaRootID(node.container_id().value()));
  }

  semantic_provider_->Update(std::move(node));
}

void AccessibilityBridgeFuchsiaImpl::DeleteNode(uint32_t node_id) {
  uint32_t fuchsia_node_id = MaybeToFuchsiaRootID(node_id);
  semantic_provider_->Delete(fuchsia_node_id);

  // If we have deleted the root node, we should also clear root_node_id_.
  if (fuchsia_node_id == AXFuchsiaSemanticProvider::kFuchsiaRootNodeId)
    root_node_id_.reset();
}

void AccessibilityBridgeFuchsiaImpl::OnAccessibilityHitTestResult(
    int hit_test_request_id,
    std::optional<uint32_t> result) {
  auto it = pending_hit_test_completers_.find(hit_test_request_id);
  if (it == pending_hit_test_completers_.end()) {
    return;
  }

  fuchsia_accessibility_semantics::Hit hit;
  if (result)
    hit.node_id(MaybeToFuchsiaRootID(*result));

  std::move(it->second).Run(std::move(hit));
  pending_hit_test_completers_.erase(it);
}

float AccessibilityBridgeFuchsiaImpl::GetDeviceScaleFactor() {
  return semantic_provider_->GetPixelScale();
}

void AccessibilityBridgeFuchsiaImpl::SetRootID(uint32_t root_node_id) {
  // If the root has changed, then we should delete the old root.
  //
  // If the old root was already deleted, then this operation will be a NOOP.
  // If the old root was NOT yet deleted, then this operation will delete it.
  // When the accessibility bridge client tries to delete it later (using its
  // AXUniqueID), the accessibility bridge will no longer map its id to
  // kFuchsiaRootNodeId. So, that deletion will also be a NOOP.
  //
  // In either case, this operation is safe, and the end state is that the old
  // root node is deleted.
  if (root_node_id_)
    semantic_provider_->Delete(AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);

  root_node_id_ = root_node_id;
}

bool AccessibilityBridgeFuchsiaImpl::OnAccessibilityAction(
    uint32_t node_id,
    fuchsia_accessibility_semantics::Action action) {
  // If the action was requested on the root, translate to the root node's
  // unique ID.
  if (node_id == AXFuchsiaSemanticProvider::kFuchsiaRootNodeId) {
    if (!root_node_id_)
      return false;

    node_id = *root_node_id_;
  }

  AXPlatformNode* ax_platform_node =
      AXPlatformNodeFuchsia::GetFromUniqueId(node_id);
  AXPlatformNodeFuchsia* ax_platform_node_fuchsia =
      static_cast<AXPlatformNodeFuchsia*>(ax_platform_node);

  if (!ax_platform_node_fuchsia)
    return false;

  AXActionData action_data = AXActionData();

  // The requested action is not supported.
  std::optional<ax::mojom::Action> ax_action = ConvertAction(action);
  if (!ax_action)
    return false;

  action_data.action = *ax_action;
  action_data.target_node_id = node_id;

  if (action == fuchsia_accessibility_semantics::Action::kShowOnScreen) {
    // The scroll-to-make-visible action expects coordinates in the local
    // coordinate space of |node|. So, we need to translate node's bounds to the
    // origin.
    gfx::Rect local_bounds = gfx::ToEnclosedRectIgnoringError(
        ax_platform_node_fuchsia->GetData().relative_bounds.bounds,
        kRectConversionError);
    local_bounds = gfx::Rect(local_bounds.size());

    action_data.target_rect = local_bounds;
    action_data.horizontal_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.vertical_scroll_alignment =
        ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
    action_data.scroll_behavior = ax::mojom::ScrollBehavior::kScrollIfVisible;
  }

  ax_platform_node_fuchsia->PerformAction(action_data);
  return true;
}

void AccessibilityBridgeFuchsiaImpl::OnHitTest(
    fuchsia_math::PointF point,
    AXFuchsiaSemanticProvider::Delegate::HitTestCallback callback) {
  AXPlatformNodeFuchsia* ax_platform_node_fuchsia = nullptr;

  if (root_node_id_) {
    // Target the root node.
    AXPlatformNode* ax_platform_node =
        AXPlatformNodeFuchsia::GetFromUniqueId(*root_node_id_);
    ax_platform_node_fuchsia =
        static_cast<AXPlatformNodeFuchsia*>(ax_platform_node);
  }

  if (!ax_platform_node_fuchsia) {
    fuchsia_accessibility_semantics::Hit hit;
    std::move(callback).Run(std::move(hit));
    return;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  gfx::Point target_point;
  target_point.set_x(point.x());
  target_point.set_y(point.y());
  action_data.target_point = target_point;
  action_data.hit_test_event_to_fire = ax::mojom::Event::kHitTestResult;
  action_data.request_id = next_hittest_request_id_++;

  pending_hit_test_completers_.insert_or_assign(action_data.request_id,
                                                std::move(callback));

  ax_platform_node_fuchsia->PerformAction(std::move(action_data));
}

void AccessibilityBridgeFuchsiaImpl::OnSemanticsEnabled(bool enabled) {
  if (on_semantics_enabled_)
    on_semantics_enabled_.Run(enabled);
}

bool AccessibilityBridgeFuchsiaImpl::OnSemanticsManagerConnectionClosed(
    zx_status_t status) {
  if (on_connection_closed_)
    return on_connection_closed_.Run(status);

  // If the user does not specify a callback, then we can assume no attempt to
  // reconnect should be made.
  return false;
}

void AccessibilityBridgeFuchsiaImpl::set_semantic_provider_for_test(
    std::unique_ptr<AXFuchsiaSemanticProvider> semantic_provider) {
  semantic_provider_ = std::move(semantic_provider);
}

inspect::Node AccessibilityBridgeFuchsiaImpl::GetInspectNode() {
  return inspect_node_.CreateChild(
      base::StringPrintf("AXTree-%d", next_inspect_tree_number_++));
}

void AccessibilityBridgeFuchsiaImpl::SetPixelScale(float pixel_scale) {
  semantic_provider_->SetPixelScale(pixel_scale);
}

}  // namespace ui
