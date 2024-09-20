// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager_auralinux.h"

#include <atk/atk.h>

#include <set>
#include <vector>

#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/browser_accessibility_auralinux.h"

namespace ui {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerAuraLinux(initial_tree,
                                                  node_id_delegate, delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerAuraLinux(
      BrowserAccessibilityManagerAuraLinux::GetEmptyDocument(),
      node_id_delegate, delegate);
}

BrowserAccessibilityManagerAuraLinux*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAuraLinux() {
  return static_cast<BrowserAccessibilityManagerAuraLinux*>(this);
}

BrowserAccessibilityManagerAuraLinux::BrowserAccessibilityManagerAuraLinux(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(node_id_delegate, delegate) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerAuraLinux::~BrowserAccessibilityManagerAuraLinux() {
  if (IsRootFrameManager()) {
    DCHECK(GetBrowserAccessibilityRoot());
    gfx::NativeViewAccessible obj =
        GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
    // We don't fire state:changed:defunct on every object in order to reduce
    // event noise, but it is useful for the root node of a document.
    if (ATK_IS_OBJECT(obj)) {
      atk_object_notify_state_change(obj, ATK_STATE_DEFUNCT, TRUE);
    }
  }
}

// static
AXTreeUpdate BrowserAccessibilityManagerAuraLinux::GetEmptyDocument() {
  AXNodeData empty_document;
  empty_document.id = kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAuraLinux::SetPrimaryWebContentsForWindow(
    AXNodeID node_id) {
  DCHECK_NE(node_id, kInvalidAXNodeID);
  DCHECK(GetFromID(node_id));
  DCHECK(primary_web_contents_for_window_id_ == node_id ||
         primary_web_contents_for_window_id_ == kInvalidAXNodeID);
  primary_web_contents_for_window_id_ = node_id;
}

AXNodeID BrowserAccessibilityManagerAuraLinux::GetPrimaryWebContentsForWindow()
    const {
  return primary_web_contents_for_window_id_;
}

void BrowserAccessibilityManagerAuraLinux::FireFocusEvent(AXNode* node) {
  AXTreeManager::FireFocusEvent(node);
  FireEvent(GetFromAXNode(node), ax::mojom::Event::kFocus);
}

void BrowserAccessibilityManagerAuraLinux::FireSelectedEvent(
    BrowserAccessibility* node) {
  // Some browser UI widgets, such as the omnibox popup, only send notifications
  // when they become selected. In contrast elements in a page, such as options
  // in the select element, also send notifications when they become unselected.
  // Since AXPlatformNodeAuraLinux must handle firing a platform event for the
  // unselected case, we can safely ignore the unselected case for rendered
  // elements.
  if (!node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    return;

  FireEvent(node, ax::mojom::Event::kSelection);
}

void BrowserAccessibilityManagerAuraLinux::FireBusyChangedEvent(
    BrowserAccessibility* node,
    bool is_busy) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnBusyStateChanged(is_busy);
}

void BrowserAccessibilityManagerAuraLinux::FireLoadingEvent(
    BrowserAccessibility* node,
    bool is_loading) {
  gfx::NativeViewAccessible obj = node->GetNativeViewAccessible();
  if (!ATK_IS_OBJECT(obj))
    return;

  atk_object_notify_state_change(obj, ATK_STATE_BUSY, is_loading);
  if (!is_loading)
    g_signal_emit_by_name(obj, "load_complete");
}

void BrowserAccessibilityManagerAuraLinux::FireEnabledChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnEnabledChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireExpandedEvent(
    BrowserAccessibility* node,
    bool is_expanded) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnExpandedStateChanged(
      is_expanded);
}

void BrowserAccessibilityManagerAuraLinux::FireShowingEvent(
    BrowserAccessibility* node,
    bool is_showing) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnShowingStateChanged(
      is_showing);
}

void BrowserAccessibilityManagerAuraLinux::FireInvalidStatusChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnInvalidStatusChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireAriaCurrentChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnAriaCurrentChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireEvent(BrowserAccessibility* node,
                                                     ax::mojom::Event event) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->NotifyAccessibilityEvent(
      event);
}

void BrowserAccessibilityManagerAuraLinux::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node,
    int action_request_id) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);

  switch (event_type) {
    case ax::mojom::Event::kScrolledToAnchor:
      ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnScrolledToAnchor();
      break;
    case ax::mojom::Event::kLoadComplete:
      // TODO(accessibility): While this check is theoretically what we want to
      // be the case, timing and other issues can cause it to fail. This seems
      // to impact bots rather than Orca and its users. If this proves to be a
      // real-world problem, we can investigate further and reinstate it.
      // DCHECK(
      //    !node->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));
      FireLoadingEvent(node, false);
      FireEvent(node, ax::mojom::Event::kLoadComplete);
      break;
    case ax::mojom::Event::kLoadStart:
      FireLoadingEvent(node, true);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerAuraLinux::FireNameChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnNameChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireDescriptionChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnDescriptionChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireParentChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnParentChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireReadonlyChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnReadonlyChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireSortDirectionChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnSortDirectionChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireTextAttributesChangedEvent(
    BrowserAccessibility* node) {
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnTextAttributesChanged();
}

void BrowserAccessibilityManagerAuraLinux::FireSubtreeCreatedEvent(
    BrowserAccessibility* node) {
  // Sending events during a load would create a lot of spam, don't do that.
  if (!GetTreeData().loaded)
    return;
  if (!CanEmitChildrenChanged(node))
    return;
  ToBrowserAccessibilityAuraLinux(node)->GetNode()->OnSubtreeCreated();
}

void BrowserAccessibilityManagerAuraLinux::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    const AXNode* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);

  BrowserAccessibility* wrapper = GetFromAXNode(node);
  DCHECK(wrapper);
  switch (event_type) {
    case AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireEvent(wrapper, ax::mojom::Event::kActiveDescendantChanged);
      break;
    case AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
      FireTextAttributesChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      FireEvent(wrapper, ax::mojom::Event::kCheckedStateChanged);
      break;
    case AXEventGenerator::Event::BUSY_CHANGED: {
      // We reliably get busy-changed notifications when the value of aria-busy
      // changes. We may or may not get a generated busy-changed notification
      // for the document at the start or end of a page load. For instance,
      // AXTree::Unserialize will not call NotifyNodeAttributesHaveBeenChanged
      // when the root is new, which is the case when a new document has started
      // loading. Because Orca needs the busy-changed notification to be
      // reliably fired on the document, we do so in response to load-start and
      // load-complete and suppress possible duplication here.
      if (wrapper->IsPlatformDocument())
        return;
      FireBusyChangedEvent(wrapper, wrapper->GetData().GetBoolAttribute(
                                        ax::mojom::BoolAttribute::kBusy));
      break;
    }
    case AXEventGenerator::Event::COLLAPSED:
      FireExpandedEvent(wrapper, false);
      break;
    case AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireDescriptionChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      AXNodeID focus_id = ax_tree()->GetUnignoredSelection().focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object)
        FireEvent(focus_object, ax::mojom::Event::kTextSelectionChanged);
      break;
    }
    case AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      FireEvent(wrapper, ax::mojom::Event::kDocumentTitleChanged);
      break;
    case AXEventGenerator::Event::ENABLED_CHANGED:
      FireEnabledChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::EXPANDED:
      FireExpandedEvent(wrapper, true);
      break;
    case AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireInvalidStatusChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      FireAriaCurrentChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::MENU_ITEM_SELECTED:
      FireSelectedEvent(wrapper);
      break;
    case AXEventGenerator::Event::MENU_POPUP_END:
      FireShowingEvent(wrapper, false);
      break;
    case AXEventGenerator::Event::MENU_POPUP_START:
      FireShowingEvent(wrapper, true);
      break;
    case AXEventGenerator::Event::NAME_CHANGED:
      FireNameChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::PARENT_CHANGED:
      FireParentChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::READONLY_CHANGED:
      FireReadonlyChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      // Before firing the platform event, check to see that the object's
      // properties still support range values, because some of the properties
      // may have been updated after the event was generated.
      if (wrapper->GetData().IsRangeValueSupported()) {
        FireEvent(wrapper, ax::mojom::Event::kValueChanged);
      }
      break;
    case AXEventGenerator::Event::ALERT:
    case AXEventGenerator::Event::ROLE_CHANGED: {
      // In ATK, there is no role change event, and instead, role changes are
      // mapped to a subtree being removed and re-added. Since the AXNodeID
      // (and therefore the AXNode) in such cases may remain the same, this must
      // be done manually.
      AXPlatformNodeAuraLinux* platform_node =
          ToBrowserAccessibilityAuraLinux(wrapper)->GetNode();
      platform_node->OnSubtreeWillBeDeleted();
      platform_node->OnSubtreeCreated();
      break;
    }
    case AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireEvent(wrapper, ax::mojom::Event::kSelectedChildrenChanged);
      break;
    case AXEventGenerator::Event::SELECTED_CHANGED:
      FireSelectedEvent(wrapper);
      break;
    case AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
      DCHECK(IsSelectElement(wrapper->GetRole()));
      FireEvent(wrapper, ax::mojom::Event::kValueChanged);
      break;
    case AXEventGenerator::Event::SORT_CHANGED:
      FireSortDirectionChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::SUBTREE_CREATED:
      FireSubtreeCreatedEvent(wrapper);
      break;
    case AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      FireTextAttributesChangedEvent(wrapper);
      break;
    case AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      if (!wrapper->IsTextField())
        return;  // node no longer editable since event originally fired.
      FireEvent(wrapper, ax::mojom::Event::kValueChanged);
      break;

    // Currently unused events on this platform.
    case AXEventGenerator::Event::NONE:
    case AXEventGenerator::Event::ACCESS_KEY_CHANGED:
    case AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED:
    case AXEventGenerator::Event::ATOMIC_CHANGED:
    case AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
    case AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
    case AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
    case AXEventGenerator::Event::CHILDREN_CHANGED:
    case AXEventGenerator::Event::CONTROLS_CHANGED:
    case AXEventGenerator::Event::DETAILS_CHANGED:
    case AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
    case AXEventGenerator::Event::FOCUS_CHANGED:
    case AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case AXEventGenerator::Event::FLOW_TO_CHANGED:
    case AXEventGenerator::Event::HASPOPUP_CHANGED:
    case AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case AXEventGenerator::Event::IGNORED_CHANGED:
    case AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
    case AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case AXEventGenerator::Event::LABELED_BY_CHANGED:
    case AXEventGenerator::Event::LANGUAGE_CHANGED:
    case AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case AXEventGenerator::Event::LIVE_REGION_CHANGED:
    case AXEventGenerator::Event::LIVE_REGION_CREATED:
    case AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
    case AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
    case AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
    case AXEventGenerator::Event::ORIENTATION_CHANGED:
    case AXEventGenerator::Event::PLACEHOLDER_CHANGED:
    case AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
    case AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
    case AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
    case AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
    case AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
    case AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
    case AXEventGenerator::Event::SET_SIZE_CHANGED:
    case AXEventGenerator::Event::STATE_CHANGED:
    case AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
    case AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      break;
  }
}

void BrowserAccessibilityManagerAuraLinux::OnNodeWillBeDeleted(
    ui::AXTree* tree,
    ui::AXNode* node) {
  if (primary_web_contents_for_window_id_ == node->id()) {
    primary_web_contents_for_window_id_ = ui::kInvalidAXNodeID;
  }
  BrowserAccessibilityManager::OnNodeWillBeDeleted(tree, node);
}

void BrowserAccessibilityManagerAuraLinux::OnIgnoredWillChange(
    AXTree* tree,
    AXNode* node,
    bool is_ignored_new_value,
    bool is_changing_unignored_parents_children) {
  DCHECK_EQ(ax_tree(), tree);

  // Since AuraLinux needs to send the children-changed::remove event with the
  // index in parent, the event must be fired before the node becomes ignored.
  // children-changed:add is handled with the generated Event::IGNORED_CHANGED.
  if (is_ignored_new_value && is_changing_unignored_parents_children) {
    BrowserAccessibility* obj = GetFromID(node->id());
    if (obj && obj->GetParent()) {
      DCHECK(!obj->IsIgnored());
      if (!CanEmitChildrenChanged(obj))
        return;
      g_signal_emit_by_name(obj->GetParent(), "children-changed::remove",
                            static_cast<gint>(obj->GetIndexInParent().value()),
                            obj->GetNativeViewAccessible());
    }
  }
}

void BrowserAccessibilityManagerAuraLinux::OnSubtreeWillBeDeleted(
    AXTree* tree,
    AXNode* node) {
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!CanEmitChildrenChanged(obj))
    return;
  ToBrowserAccessibilityAuraLinux(obj)->GetNode()->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManagerAuraLinux::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  std::set<AXPlatformNode*> objs_to_update;
  CollectChangedNodesAndParentsForAtomicUpdate(tree, changes, &objs_to_update);

  for (auto* node : objs_to_update)
    static_cast<AXPlatformNodeAuraLinux*>(node)->UpdateHypertext();
}

void BrowserAccessibilityManagerAuraLinux::OnFindInPageResult(int request_id,
                                                              int match_index,
                                                              int start_id,
                                                              int start_offset,
                                                              int end_id,
                                                              int end_offset) {
  BrowserAccessibility* node = GetFromID(start_id);
  if (!node)
    return;
  AXPlatformNodeAuraLinux* platform_node =
      ToBrowserAccessibilityAuraLinux(node)->GetNode();

  // TODO(accessibility): We should support selections that span multiple
  // elements, but for now if we see a result that spans multiple elements,
  // just activate until the end of the node.
  if (end_id != start_id)
    end_offset = platform_node->GetHypertext().size();

  platform_node->ActivateFindInPageResult(start_offset, end_offset);
}

void BrowserAccessibilityManagerAuraLinux::OnFindInPageTermination() {
  static_cast<BrowserAccessibilityAuraLinux*>(GetBrowserAccessibilityRoot())
      ->GetNode()
      ->TerminateFindInPage();
}

bool BrowserAccessibilityManagerAuraLinux::CanEmitChildrenChanged(
    BrowserAccessibility* node) const {
  if (!node || !ShouldFireEventForNode(node))
    return false;
  BrowserAccessibility* parent = node->PlatformGetParent();
  if (!parent || parent->IsLeaf())
    return false;
  return true;
}

}  // namespace ui
