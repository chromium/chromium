// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/browser_accessibility_manager_fuchsia.h"

#include <lib/inspect/component/cpp/component.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"
#include "ui/accessibility/platform/fuchsia/browser_accessibility_fuchsia.h"

namespace ui {
namespace {
// Accessibility bridge instance to use for tests, if set.
AccessibilityBridgeFuchsia* g_accessibility_bridge_for_test = nullptr;
}  // namespace

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(initial_tree, node_id_delegate,
                                                delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerFuchsia(
      BrowserAccessibilityManagerFuchsia::GetEmptyDocument(), node_id_delegate,
      delegate);
}

BrowserAccessibilityManagerFuchsia::BrowserAccessibilityManagerFuchsia(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(node_id_delegate, delegate) {
  Initialize(initial_tree);

  AccessibilityBridgeFuchsia* accessibility_bridge = GetAccessibilityBridge();
  if (accessibility_bridge) {
    inspect_node_ = accessibility_bridge->GetInspectNode();
    tree_dump_node_ = inspect_node_.CreateLazyNode("tree-data", [this]() {
      inspect::Inspector inspector;

      auto str = ax_tree()->ToString();
      auto str_capacity = str.capacity();
      inspector.GetRoot().CreateString(GetTreeID().ToString(), std::move(str),
                                       &inspector);

      // Test to check if the string fit in memory.
      if (inspector.GetStats().failed_allocations > 0) {
        ZX_LOG(WARNING, ZX_OK)
            << "Inspector had failed allocations. Some semantic tree data may "
               "be missing. Size of the string we tried to store: "
            << str_capacity << " bytes";
      }

      return fpromise::make_ok_promise(inspector);
    });
  }
}

BrowserAccessibilityManagerFuchsia::~BrowserAccessibilityManagerFuchsia() =
    default;

AccessibilityBridgeFuchsia*
BrowserAccessibilityManagerFuchsia::GetAccessibilityBridge() const {
  // !!! Safety warning !!! This function is directly called during the
  // parent class destructor. As such, it shouldn't depend on any member
  // variables from this class, as they have already been destroyed.
  //
  // Failing to follow this rule would be undefined behavior, and can lead to
  // unsafe/unexpected behaviors. See ASAN options
  // `-fsanitize-memory-use-after-dtor`
  if (g_accessibility_bridge_for_test) {
    return g_accessibility_bridge_for_test;
  }

  gfx::NativeWindow top_level_native_window =
      delegate_ ? delegate_->GetTopLevelNativeWindow() : gfx::NativeWindow();

  aura::Window* accessibility_bridge_key =
      top_level_native_window ? top_level_native_window->GetRootWindow()
                              : nullptr;
  if (!accessibility_bridge_key) {
    return nullptr;
  }

  AccessibilityBridgeFuchsiaRegistry* accessibility_bridge_registry =
      AccessibilityBridgeFuchsiaRegistry::GetInstance();
  DCHECK(accessibility_bridge_registry);

  return accessibility_bridge_registry->GetAccessibilityBridge(
      accessibility_bridge_key);
}

void BrowserAccessibilityManagerFuchsia::FireFocusEvent(AXNode* node) {
  AXTreeManager::FireFocusEvent(node);

  if (!GetAccessibilityBridge())
    return;

  BrowserAccessibilityFuchsia* new_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(GetFromAXNode(node));

  BrowserAccessibilityFuchsia* old_focus_fuchsia =
      ToBrowserAccessibilityFuchsia(GetFromAXNode(GetLastFocusedNode()));

  if (old_focus_fuchsia)
    old_focus_fuchsia->OnDataChanged();

  if (new_focus_fuchsia)
    new_focus_fuchsia->OnDataChanged();
}

// static
AXTreeUpdate BrowserAccessibilityManagerFuchsia::GetEmptyDocument() {
  AXNodeData empty_document;
  empty_document.id = kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerFuchsia::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node,
    int action_request_id) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);

  // Blink fires a hover event on the result of a hit test, so we should process
  // it here.
  if (event_type == ax::mojom::Event::kHover)
    OnHitTestResult(action_request_id, node);
}

void BrowserAccessibilityManagerFuchsia::OnHitTestResult(
    int action_request_id,
    BrowserAccessibility* node) {
  if (!GetAccessibilityBridge())
    return;

  std::optional<uint32_t> hit_result_id;

  if (node) {
    BrowserAccessibilityFuchsia* hit_result =
        ToBrowserAccessibilityFuchsia(node);
    DCHECK(hit_result);
    hit_result_id = hit_result->GetFuchsiaNodeID();
  }

  GetAccessibilityBridge()->OnAccessibilityHitTestResult(action_request_id,
                                                         hit_result_id);
}

void BrowserAccessibilityManagerFuchsia::UpdateDeviceScaleFactor() {
  AccessibilityBridgeFuchsia* accessibility_bridge = GetAccessibilityBridge();
  if (accessibility_bridge)
    device_scale_factor_ = accessibility_bridge->GetDeviceScaleFactor();
  else
    BrowserAccessibilityManager::UpdateDeviceScaleFactor();
}

// static
void BrowserAccessibilityManagerFuchsia::SetAccessibilityBridgeForTest(
    AccessibilityBridgeFuchsia* accessibility_bridge_for_test) {
  // Only allow transition from nullptr to non-nullptr, or vice versa.
  CHECK(!g_accessibility_bridge_for_test || !accessibility_bridge_for_test)
      << "Setting the accessibility bridge to two different values is not "
         "allowed.";
  g_accessibility_bridge_for_test = accessibility_bridge_for_test;
}

}  // namespace ui
