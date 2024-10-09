// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update_util.h"
#include "ui/accessibility/platform/ax_node_id_delegate.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/buildflags.h"


namespace ui {

AXTreeUpdate MakeAXTreeUpdateForTesting(
    const AXNodeData& node1,
    const AXNodeData& node2 /* = AXNodeData() */,
    const AXNodeData& node3 /* = AXNodeData() */,
    const AXNodeData& node4 /* = AXNodeData() */,
    const AXNodeData& node5 /* = AXNodeData() */,
    const AXNodeData& node6 /* = AXNodeData() */,
    const AXNodeData& node7 /* = AXNodeData() */,
    const AXNodeData& node8 /* = AXNodeData() */,
    const AXNodeData& node9 /* = AXNodeData() */,
    const AXNodeData& node10 /* = AXNodeData() */,
    const AXNodeData& node11 /* = AXNodeData() */,
    const AXNodeData& node12 /* = AXNodeData() */,
    const AXNodeData& node13 /* = AXNodeData() */,
    const AXNodeData& node14 /* = AXNodeData() */) {
  static base::NoDestructor<AXNodeData> empty_data;
  int32_t no_id = empty_data->id;

  AXTreeUpdate update;
  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;
  tree_data.parent_tree_id = AXTreeIDUnknown();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = node1.id;
  update.nodes.push_back(node1);
  if (node2.id != no_id)
    update.nodes.push_back(node2);
  if (node3.id != no_id)
    update.nodes.push_back(node3);
  if (node4.id != no_id)
    update.nodes.push_back(node4);
  if (node5.id != no_id)
    update.nodes.push_back(node5);
  if (node6.id != no_id)
    update.nodes.push_back(node6);
  if (node7.id != no_id)
    update.nodes.push_back(node7);
  if (node8.id != no_id)
    update.nodes.push_back(node8);
  if (node9.id != no_id)
    update.nodes.push_back(node9);
  if (node10.id != no_id)
    update.nodes.push_back(node10);
  if (node11.id != no_id)
    update.nodes.push_back(node11);
  if (node12.id != no_id)
    update.nodes.push_back(node12);
  if (node13.id != no_id)
    update.nodes.push_back(node13);
  if (node14.id != no_id)
    update.nodes.push_back(node14);
  return update;
}

BrowserAccessibilityFindInPageInfo::BrowserAccessibilityFindInPageInfo()
    : request_id(-1),
      match_index(-1),
      start_id(-1),
      start_offset(0),
      end_id(-1),
      end_offset(-1),
      active_request_id(-1) {}

#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  BrowserAccessibilityManager* manager =
      new BrowserAccessibilityManager(node_id_delegate, delegate);
  manager->Initialize(initial_tree);
  return manager;
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  BrowserAccessibilityManager* manager =
      new BrowserAccessibilityManager(node_id_delegate, delegate);
  manager->Initialize(BrowserAccessibilityManager::GetEmptyDocument());
  return manager;
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::FromID(
    AXTreeID ax_tree_id) {
  DCHECK(ax_tree_id != AXTreeIDUnknown());
  auto* manager = AXTreeManager::FromID(ax_tree_id);
  // If `manager` does not maintain a list of platform objects (such as
  // `BrowserAccessibility`) corresponding to each `AXNode` in its managed tree,
  // then we can't cast it to one that does, in this case a
  // `BrowserAccessibilityManager`.
  if (!manager || !manager->IsPlatformTreeManager()) {
    return nullptr;
  }
  return static_cast<BrowserAccessibilityManager*>(manager);
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate)
    : AXPlatformTreeManager(std::make_unique<AXSerializableTree>()),
      delegate_(delegate),
      user_is_navigating_away_(false),
      device_scale_factor_(1.0f),
      use_custom_device_scale_factor_for_testing_(false),
      node_id_delegate_(node_id_delegate) {}

BrowserAccessibilityManager::~BrowserAccessibilityManager() = default;

// A flag for use in tests to ensure events aren't suppressed or delayed.
// static
bool BrowserAccessibilityManager::never_suppress_or_delay_events_for_testing_ =
    false;

// static
AXTreeUpdate BrowserAccessibilityManager::GetEmptyDocument() {
  AXNodeData empty_document;
  empty_document.id = kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManager::FireFocusEventsIfNeeded() {
  if (!CanFireEvents())
    return;

  BrowserAccessibility* focus = GetFocus();
  // If |focus| is nullptr it means that we have no way of knowing where the
  // focus is.
  //
  // One case when this would happen is when the current tree hasn't connected
  // to its parent tree yet. That would mean that we have no way of getting to
  // the top document which holds global focus information for the whole page.
  //
  // Note that if there is nothing focused on the page, then the focus should
  // not be nullptr. The rootnode of the top document should be focused instead.
  if (!focus)
    return;

  // Don't fire focus events if the window itself doesn't have focus.
  // Bypass this check for some tests.
  if (!never_suppress_or_delay_events_for_testing_ &&
      !AXTreeManager::GetFocusChangeCallbackForTesting()) {
    if (delegate_ && !delegate_->AccessibilityViewHasFocus())
      return;
  }

  AXNode* last_focused_node = GetLastFocusedNode();
  if (focus != GetFromAXNode(last_focused_node)) {
    // Wait until navigation is complete or stopped, before attempting to move
    // the accessibility focus.
    if (!user_is_navigating_away_) {
      FireFocusEvent(focus->node());
    }
    SetLastFocusedNode(focus->node());
  }
}

bool BrowserAccessibilityManager::CanFireEvents() const {
  if (!AXTreeManager::CanFireEvents())
    return false;

  BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
  // If the check below is changed to a DCHECK, it will fail when running
  // http/tests/devtools/resource-tree/resource-tree-frame-in-crafted-frame.js
  // on linux. The parent `RenderFrameHostImpl` might not have an AXTreeID
  // that isn't `AXTreeIDUnknown()`.
  if (!root_manager)
    return false;

  // Do not fire events when the page is frozen inside the back/forward cache.
  // Rationale for the back/forward cache behavior:
  // https://docs.google.com/document/d/1_jaEAXurfcvriwcNU-5u0h8GGioh0LelagUIIGFfiuU/
  return !delegate_ ||  // Can be null in unit tests.
         delegate_->CanFireAccessibilityEvents();
}

void BrowserAccessibilityManager::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    const AXNode* node) {
  if (!generated_event_callback_for_testing_.is_null()) {
    generated_event_callback_for_testing_.Run(this, event_type, node->id());
  }

  // Currently, this method is only used to fire ARIA notification events.
  if (event_type != AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED) {
    return;
  }

  auto* wrapper = GetFromAXNode(node);
  DCHECK(wrapper);

  const auto& node_data = wrapper->GetData();

  const auto& announcements = node_data.GetStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationAnnouncements);
  const auto& notification_ids = node_data.GetStringListAttribute(
      ax::mojom::StringListAttribute::kAriaNotificationIds);

  const auto& interrupt_properties = node_data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties);
  const auto& priority_properties = node_data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties);

  DCHECK_EQ(announcements.size(), notification_ids.size());
  DCHECK_EQ(announcements.size(), interrupt_properties.size());
  DCHECK_EQ(announcements.size(), priority_properties.size());

  for (std::size_t i = 0; i < announcements.size(); ++i) {
    FireAriaNotificationEvent(wrapper, announcements[i], notification_ids[i],
                              static_cast<ax::mojom::AriaNotificationInterrupt>(
                                  interrupt_properties[i]),
                              static_cast<ax::mojom::AriaNotificationPriority>(
                                  priority_properties[i]));
  }
}

BrowserAccessibility* BrowserAccessibilityManager::GetBrowserAccessibilityRoot()
    const {
  AXNode* root = GetRoot();
  return root ? GetFromID(root->id()) : nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromAXNode(
    const AXNode* node) const {
  // TODO(benjamin.beaudry): Consider moving `GetFromId` to
  // `AXPlatformTreeManager`.
  if (!node)
    return nullptr;
  // TODO(aleventhal) Why would node->GetManager() return null?
  // TODO(aleventhal) Should we just use |this| as the manager in most cases? It
  // looks like node->GetManager() may be slow because of AXTreeID usage.
  if (const AXTreeManager* manager = node->GetManager()) {
    // If `manager` does not maintain a list of platform objects (such as
    // `BrowserAccessibility`) corresponding to each `AXNode` in its managed
    // tree, then we can't cast it to one that does, in this case a
    // `BrowserAccessibilityManager`.
    if (manager->IsPlatformTreeManager()) {
      return static_cast<const BrowserAccessibilityManager*>(manager)
          ->GetFromID(node->id());
    }
    return nullptr;
  }
  return GetFromID(node->id());
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromID(int32_t id) const {
  if (id == kInvalidAXNodeID) {
    return nullptr;
  }
  const auto iter = id_wrapper_map_.find(id);
  if (iter != id_wrapper_map_.end()) {
    DCHECK(iter->second);
    return iter->second.get();
  }

  return nullptr;
}

BrowserAccessibility*
BrowserAccessibilityManager::GetParentNodeFromParentTreeAsBrowserAccessibility()
    const {
  AXNode* parent = GetParentNodeFromParentTree();
  if (!parent)
    return nullptr;

  // TODO(accessibility) Try to remove this redundant lookup. The call to
  // `GetParentNodeFromParentTree` already retrieved the parent manager.
  AXTreeManager* parent_manager = GetParentManager();
  DCHECK(parent_manager) << "Impossible to have null parent_manager if we "
                            "already have a parent AXNode.";

  // There is a chance that the parent manager is not a
  // `BrowserAccessibilityManager` since the parent of the
  // manager that is on the root frame will be a
  // `ViewsAXTreeManager`. The manager could also own an `AXTree` with
  // generated content, which is currently not a platform tree manager. In those
  // cases, we should return nullptr since doing the cast will fail and result
  // in undefined behavior.
  if (IsRootFrameManager() || !IsPlatformTreeManager()) {
    return nullptr;
  }
  BrowserAccessibilityManager* parent_manager_wrapper =
      static_cast<BrowserAccessibilityManager*>(parent_manager);
  BrowserAccessibility* parent_node =
      parent_manager_wrapper->GetFromAXNode(parent);
  DCHECK_EQ(parent_node->manager(), parent_manager_wrapper);
  DCHECK_NE(parent_node->manager(), this);
  return parent_node;
}

void BrowserAccessibilityManager::UpdateAttributesOnParent(AXNode* parent) {
  BrowserAccessibility* parent_wrapper = GetFromAXNode(parent);
  if (!parent_wrapper)
    return;
  parent_wrapper->OnDataChanged();
  parent_wrapper->UpdatePlatformAttributes();
}

BrowserAccessibility* BrowserAccessibilityManager::GetPopupRoot() const {
  DCHECK_LE(popup_root_ids_.size(), 1u);
  if (popup_root_ids_.size() == 1) {
    BrowserAccessibility* node = GetFromID(*popup_root_ids_.begin());
    if (node) {
      DCHECK(node->GetRole() == ax::mojom::Role::kGroup);
      return node;
    }
  }
  return nullptr;
}

void BrowserAccessibilityManager::OnWindowFocused() {
  if (IsRootFrameManager())
    FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::OnWindowBlurred() {
  if (IsRootFrameManager())
    SetLastFocusedNode(nullptr);
}

void BrowserAccessibilityManager::UserIsNavigatingAway() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::UserIsReloading() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::NavigationSucceeded() {
  user_is_navigating_away_ = false;
  // Do not call FireFocusEventsIfNeeded() yet -- wait until first call
  // of OnAccessibilityEvents(), which will occur when kLoadStart is fired from
  // the renderer, at which point there will be an AXTreeID().
}

void BrowserAccessibilityManager::NavigationFailed() {
  user_is_navigating_away_ = false;
  FireFocusEventsIfNeeded();
}

void BrowserAccessibilityManager::DidStopLoading() {
  user_is_navigating_away_ = false;
  FireFocusEventsIfNeeded();
}

bool BrowserAccessibilityManager::UseRootScrollOffsetsWhenComputingBounds() {
  return use_root_scroll_offsets_when_computing_bounds_;
}

void BrowserAccessibilityManager ::
    SetUseRootScrollOffsetsWhenComputingBoundsForTesting(bool use) {
  use_root_scroll_offsets_when_computing_bounds_ = use;
}

bool BrowserAccessibilityManager::OnAccessibilityEvents(
    const AXUpdatesAndEvents& details) {
  TRACE_EVENT0(
      "accessibility",
      is_post_load_
          ? "BrowserAccessibilityManager::OnAccessibilityEvents"
          : "BrowserAccessibilityManager::OnAccessibilityEventsLoading");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.BrowserAccessibilityManager::"
      "OnAccessibilityEvents2");

#if DCHECK_IS_ON()
  base::AutoReset<bool> auto_reset(&in_on_accessibility_events_, true);
#endif  // DCHECK_IS_ON()

  // Update the cached device scale factor.
  if (!use_custom_device_scale_factor_for_testing_)
    UpdateDeviceScaleFactor();

  // Optionally merge multiple tree updates into fewer updates.
  const std::vector<AXTreeUpdate>* tree_updates = &details.updates;
  std::vector<AXTreeUpdate> merged_tree_updates;
  if (MergeAXTreeUpdates(details.updates, &merged_tree_updates)) {
    tree_updates = &merged_tree_updates;
  }

  // Process all changes to the accessibility tree first.
  for (const AXTreeUpdate& tree_update : *tree_updates) {
    if (!ax_tree()->Unserialize(tree_update)) {
      // This is a fatal error, but if there is a delegate, it will handle the
      // error result and recover by re-creating the manager. After a max
      // threshold number of errors is reached, it will crash the browser.
      CHECK(!ax_tree()->error().empty())
          << "A failed serialization didn't supply the error via "
             "AXTree::RecordError().";
      if (!delegate_)
        CHECK(false) << ax_tree()->error();
      return false;
    }

    // It's a bug if we got an update containing more nodes than
    // the size of the resulting tree. If Unserialize succeeded that
    // means a node just got repeated or something harmless like that,
    // but it should still be investigated and could be the sign of a
    // performance issue.
    DCHECK_LE(static_cast<int>(tree_update.nodes.size()), ax_tree()->size());
    // Every node in the AXTree must also be in BAM's map. However, the BAM map
    // can have extra nodes, specifically extra mac nodes from AXTableInfo.
    DCHECK_GE(static_cast<int>(id_wrapper_map_.size()), ax_tree()->size());
  }

  DCHECK(ax_tree()->root());

  EnsureParentConnectionIfNotRootManager();

  if (!CanFireEvents()) {
    // TODO(accessibility) Change AXEventGenerator() to avoid doing any work
    // and avoid queuing any events when CanFireEvents() is false.
    for (const AXEvent& event : details.events) {
      if (event.event_type != ax::mojom::Event::kLoadComplete)
        defer_load_complete_event_ = true;
    }
    event_generator().ClearEvents();
    return true;
  }

  BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
  DCHECK(root_manager) << "Cannot have detached document here, as "
                          "CanFireEvents() must return false in that case.";

#if defined(AX_FAIL_FAST_BUILD)
  AXTreeID parent_id = GetParentTreeID();
  bool has_parent_id = parent_id != AXTreeIDUnknown();
  BrowserAccessibilityManager* parent_manager =
      has_parent_id ? BrowserAccessibilityManager::FromID(parent_id) : nullptr;
  if (IsRootFrameManager()) {
    CHECK(!has_parent_id) << "The root frame must be parentless, root url = "
                          << GetTreeData().url << "\nSupposed parent = "
                          << (parent_manager
                                  ? parent_manager->GetTreeData().url
                                  : "[not in map for parent_tree_id]");
    CHECK(!connected_to_parent_tree_node_)
        << "Root manager must not be connected to a parent tree node.";
  } else {
    CHECK(parent_manager) << "Non-root trees must have a parent manager to "
                             "reach this code, otherwise CanFireEvents() "
                             "should have returned false, has_parent_id = "
                          << has_parent_id
                          << "\nCurrent url = " << GetTreeData().url;
    CHECK(connected_to_parent_tree_node_)
        << "Must be connected to parent tree node, otherwise could not reach "
           "here, due to CanFireEvents() check above.";
  }
#endif

  // Allow derived classes to do event pre-processing.
  BeforeAccessibilityEvents();

  bool received_load_complete_event = false;

  // If an earlier load complete event was suppressed, fire it now.
  if (defer_load_complete_event_) {
    received_load_complete_event = true;
    defer_load_complete_event_ = false;
    FireBlinkEvent(ax::mojom::Event::kLoadComplete,
                   GetBrowserAccessibilityRoot(), -1);
  }

  // Fire any events related to changes to the tree that come from ancestors of
  // the currently-focused node. We do this so that screen readers are made
  // aware of changes in the tree which might be relevant to subsequent events
  // on the focused node, such as the focused node being a descendant of a
  // reparented node or a newly-shown dialog box.
  BrowserAccessibility* focus = GetFocus();
  std::vector<AXEventGenerator::TargetedEvent> deferred_events;
  for (const auto& targeted_event : event_generator()) {
    BrowserAccessibility* event_target = GetFromID(targeted_event.node_id);
    DCHECK(event_target) << "No event target for " << targeted_event.node_id;

    event_target = RetargetBrowserAccessibilityForEvents(
        event_target, RetargetEventType::RetargetEventTypeGenerated);
    if (!event_target)
      continue;  // Drop the event if RetargetForEvents() returns nullptr.
    if (!event_target->CanFireEvents())
      continue;

    // IsDescendantOf() also returns true in the case of equality.
    if (focus && focus != event_target && focus->IsDescendantOf(event_target)) {
      FireGeneratedEvent(targeted_event.event_params->event,
                         event_target->node());
    } else {
      deferred_events.push_back(targeted_event);
    }
  }

  // Screen readers might not process events related to the currently-focused
  // node if they are not aware that node is now focused, so fire a focus event
  // before firing any other events on that node. No focus event will be fired
  // if the window itself isn't focused or if focus hasn't changed.
  //
  // We need to fire focus events specifically from the root manager, since we
  // need the top document's delegate to check if its view has focus.
  //
  // If this manager is disconnected from the top document, then root_manager
  // will be a null pointer and this code will not be reached.
  root_manager->FireFocusEventsIfNeeded();

  // Now fire all of the rest of the generated events we previously deferred.
  for (const auto& targeted_event : deferred_events) {
    BrowserAccessibility* event_target = GetFromID(targeted_event.node_id);
    DCHECK(event_target) << "No event target for " << targeted_event.node_id;

    event_target = RetargetBrowserAccessibilityForEvents(
        event_target, RetargetEventType::RetargetEventTypeGenerated);
    if (!event_target)
      continue;  // Drop the event if RetargetForEvents() returns nullptr.
    if (!event_target->CanFireEvents())
      continue;

    FireGeneratedEvent(targeted_event.event_params->event,
                       event_target->node());
  }
  event_generator().ClearEvents();

  // Fire events from Blink.
  for (const AXEvent& event : details.events) {
    // Fire the native event.
    BrowserAccessibility* event_target = GetFromID(event.id);
    DCHECK(event_target) << "No event target for " << event.id
                         << " with event type " << event.event_type;
    RetargetEventType type =
        event.event_type == ax::mojom::Event::kHover
            ? RetargetEventType::RetargetEventTypeBlinkHover
            : RetargetEventType::RetargetEventTypeBlinkGeneral;
    BrowserAccessibility* retargeted =
        RetargetBrowserAccessibilityForEvents(event_target, type);
    if (!retargeted)
      continue;  // Drop the event if RetargetForEvents() returns nullptr.
    if (!retargeted->CanFireEvents())
      continue;

    if (event.event_type == ax::mojom::Event::kHover)
      root_manager->CacheHitTestResult(event_target);

    if (event.event_type == ax::mojom::Event::kLoadComplete) {
      DCHECK_EQ(event_target, GetBrowserAccessibilityRoot());
      DCHECK(event_target->IsPlatformDocument());

      // Don't fire multiple load-complete events. One may have been added by
      // RenderAccessibilityImpl::SendPendingAccessibilityEvents, and firing
      // multiple events can result in screen readers double-presenting the
      // load and/or interrupting speech. See, for instance, crbug.com/1352464.
      if (received_load_complete_event)
        continue;

      received_load_complete_event = true;
    }

    if (event.event_type == ax::mojom::Event::kLoadStart) {
      DCHECK_EQ(event_target, GetBrowserAccessibilityRoot());
      DCHECK(event_target->IsPlatformDocument());
      // If we already have a load-complete event, the load-start event is no
      // longer relevant. In addition, some code checks for the presence of
      // the "busy" state when firing a platform load-start event. If the page
      // is no longer loading, this state will have been removed and the check
      // will fail.
      if (received_load_complete_event)
        continue;  // Skip firing load start event.
    }

    FireBlinkEvent(event.event_type, retargeted, event.action_request_id);
  }

  if (received_load_complete_event) {
    // Fire a focus event after the document has finished loading, but after all
    // the platform independent events have already fired.
    // Some screen readers need a focus event in order to work properly.
    FireFocusEventsIfNeeded();

    // Perform the initial run of language detection.
    ax_tree()->language_detection_manager->DetectLanguages();
    ax_tree()->language_detection_manager->LabelLanguages();

    // After initial language detection, enable language detection for future
    // content updates in order to support dynamic content changes.
    //
    // If the LanguageDetectionDynamic feature flag is not enabled then this
    // is a no-op.
    ax_tree()->language_detection_manager->RegisterLanguageDetectionObserver();
  }

  // Allow derived classes to do event post-processing.
  FinalizeAccessibilityEvents();

  if (!is_post_load_) {
    for (const AXEvent& event : details.events) {
      if (event.event_type == ax::mojom::Event::kLoadComplete) {
        is_post_load_ = true;
      }
    }
  }

  return true;
}

void BrowserAccessibilityManager::BeforeAccessibilityEvents() {}

void BrowserAccessibilityManager::FinalizeAccessibilityEvents() {}

void BrowserAccessibilityManager::OnLocationChanges(
    const AXLocationAndScrollUpdates& changes) {
  TRACE_EVENT0("accessibility",
               is_post_load_
                   ? "BrowserAccessibilityManager::OnLocationChanges"
                   : "BrowserAccessibilityManager::OnLocationChangesLoading");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.BrowserAccessibilityManager::"
      "OnLocationChanges");
  bool can_fire_events = CanFireEvents();
  for (auto& change : changes.scroll_changes) {
    BrowserAccessibility* obj = GetFromID(change.id);
    if (!obj) {
      continue;
    }

    AXNode* node = obj->node();
    int old_scrollx, old_scrolly;
    node->GetScrollInfo(&old_scrollx, &old_scrolly);
    node->SetScrollInfo(change.scroll_x, change.scroll_y);

    if (can_fire_events) {
      if (change.scroll_x != old_scrollx) {
        FireGeneratedEvent(
            ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED,
            node);
      }
      if (change.scroll_y != old_scrolly) {
        FireGeneratedEvent(
            ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED,
            node);
      }
    }
  }

  for (auto& change : changes.location_changes) {
    BrowserAccessibility* obj = GetFromID(change.id);
    if (!obj) {
      continue;
    }
    AXNode* node = obj->node();
    node->SetLocation(change.new_location.offset_container_id,
                      change.new_location.bounds,
                      change.new_location.transform.get());
  }

  // Only send location change events when the page is not in back/forward
  // cache.
  if (can_fire_events && !changes.location_changes.empty()) {
    SendLocationChangeEvents(changes.location_changes);
  }

  // Only send location change callback when there's actually changed locations.
  // Required for tests to detect location change that's not scrolling.
  if (!location_change_callback_for_testing_.is_null() &&
      !changes.location_changes.empty()) {
    location_change_callback_for_testing_.Run();
  }
}

void BrowserAccessibilityManager::SendLocationChangeEvents(
    const std::vector<AXLocationChange>& changes) {
  for (auto& change : changes) {
    BrowserAccessibility* obj = GetFromID(change.id);
    if (obj)
      obj->OnLocationChanged();
  }
}

void BrowserAccessibilityManager::OnFindInPageResult(int request_id,
                                                     int match_index,
                                                     int start_id,
                                                     int start_offset,
                                                     int end_id,
                                                     int end_offset) {
  find_in_page_info_.request_id = request_id;
  find_in_page_info_.match_index = match_index;
  find_in_page_info_.start_id = start_id;
  find_in_page_info_.start_offset = start_offset;
  find_in_page_info_.end_id = end_id;
  find_in_page_info_.end_offset = end_offset;

  if (find_in_page_info_.active_request_id == request_id)
    ActivateFindInPageResult(request_id);
}

void BrowserAccessibilityManager::ActivateFindInPageResult(int request_id) {
  find_in_page_info_.active_request_id = request_id;
  if (find_in_page_info_.request_id != request_id)
    return;

  BrowserAccessibility* node = GetFromID(find_in_page_info_.start_id);
  if (!node)
    return;

  // If an ancestor of this node is a leaf node, or if this node is ignored,
  // fire the notification on that.
  node = node->PlatformGetLowestPlatformAncestor();
  DCHECK(node);

  // The "scrolled to anchor" notification is a great way to get a
  // screen reader to jump directly to a specific location in a document.
  FireBlinkEvent(ax::mojom::Event::kScrolledToAnchor, node,
                 /*action_request_id=*/-1);
}

BrowserAccessibility* BrowserAccessibilityManager::GetActiveDescendant(
    BrowserAccessibility* node) const {
  if (!node)
    return nullptr;

  AXNodeID active_descendant_id;
  BrowserAccessibility* active_descendant = nullptr;
  if (node->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                            &active_descendant_id)) {
    active_descendant = node->manager()->GetFromID(active_descendant_id);
  }

  // TODO(crbug.com/40864907): This code should be removed in favor of the right
  // computation of the active descendant in Blink.
  if (node->GetRole() == ax::mojom::Role::kComboBoxSelect) {
    BrowserAccessibility* child = node->InternalGetFirstChild();
    if (child && child->GetRole() == ax::mojom::Role::kMenuListPopup &&
        !child->IsInvisibleOrIgnored()) {
      // The active descendant is found on the menu list popup, i.e. on the
      // actual list and not on the button that opens it.
      // If there is no active descendant, focus should stay on the button so
      // that Windows screen readers would enable their virtual cursor.
      // Do not expose an activedescendant in a hidden/collapsed list, as
      // screen readers expect the focus event to go to the button itself.
      // Note that the AX hierarchy in this case is strange -- the active
      // option is the only visible option, and is inside an invisible list.
      if (child->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                 &active_descendant_id)) {
        active_descendant = child->manager()->GetFromID(active_descendant_id);
      }
    }
  }

  if (active_descendant && !active_descendant->IsInvisibleOrIgnored())
    return active_descendant;

  return node;
}

std::vector<BrowserAccessibility*> BrowserAccessibilityManager::GetAriaControls(
    const BrowserAccessibility* focus) const {
  if (!focus)
    return {};

  std::vector<BrowserAccessibility*> aria_control_nodes;
  for (const auto& id :
       focus->GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds)) {
    if (focus->manager()->GetFromID(id))
      aria_control_nodes.push_back(focus->manager()->GetFromID(id));
  }

  return aria_control_nodes;
}

bool BrowserAccessibilityManager::NativeViewHasFocus() {
  AXPlatformTreeManagerDelegate* delegate = GetDelegateFromRootManager();
  return delegate && delegate->AccessibilityViewHasFocus();
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus() const {
  BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
  if (!root_manager) {
    // We can't retrieved the globally focused object since we don't have access
    // to the top document. If we return the focus in the current or a
    // descendent tree, it might be wrong, since the top document might have
    // another frame as the tree with the focus.
    return nullptr;
  }

  AXTreeID focused_tree_id = root_manager->GetTreeData().focused_tree_id;
  BrowserAccessibilityManager* focused_manager = nullptr;
  if (focused_tree_id != AXTreeIDUnknown()) {
    focused_manager = BrowserAccessibilityManager::FromID(focused_tree_id);
  }

  // BrowserAccessibilityManager::FromID(focused_tree_id) may return nullptr if
  // the tree is not created or has been destroyed. In this case, we don't
  // really know where the focus is, so we should return nullptr. However, due
  // to a bug in RenderFrameHostImpl this is currently not possible.
  //
  // TODO(nektar): Fix All the issues identified in crbug.com/956748
  if (!focused_manager)
    return GetFocusFromThisOrDescendantFrame();

  return focused_manager->GetFocusFromThisOrDescendantFrame();
}

BrowserAccessibility*
BrowserAccessibilityManager::GetFocusFromThisOrDescendantFrame() const {
  AXNodeID focus_id = GetTreeData().focus_id;
  BrowserAccessibility* obj = GetFromID(focus_id);
  // If nothing is focused, then the top document has the focus.
  if (!obj)
    return GetBrowserAccessibilityRoot();

  if (obj->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    AXTreeID child_tree_id = AXTreeID::FromString(
        obj->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    const BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(child_tree_id);
    if (child_manager)
      return child_manager->GetFocusFromThisOrDescendantFrame();
  }

  return GetActiveDescendant(obj);
}

void BrowserAccessibilityManager::Blur(const BrowserAccessibility& node) {
  if (!delegate_) {
    return;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kBlur;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetFocus(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.SetFocus"));

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = node.GetId();
  if (!delegate_->AccessibilityViewHasFocus())
    delegate_->AccessibilityViewSetFocus();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetSequentialFocusNavigationStartingPoint(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action =
      ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetGeneratedEventCallbackForTesting(
    const GeneratedEventCallbackForTesting& callback) {
  generated_event_callback_for_testing_ = callback;
}

void BrowserAccessibilityManager::SetLocationChangeCallbackForTesting(
    const base::RepeatingClosure& callback) {
  location_change_callback_for_testing_ = callback;
}

// static
void BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting() {
  never_suppress_or_delay_events_for_testing_ = true;
}

void BrowserAccessibilityManager::Decrement(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDecrement;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  DCHECK(node.node()->data().GetDefaultActionVerb() !=
         ax::mojom::DefaultActionVerb::kNone);

  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.DoDefault"));

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::GetImageData(const BrowserAccessibility& node,
                                               const gfx::Size& max_size) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kGetImageData;
  action_data.target_node_id = node.GetId();
  action_data.target_rect = gfx::Rect(gfx::Point(), max_size);
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::Increment(const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kIncrement;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::Expand(const BrowserAccessibility& node) {
  if (!delegate_) {
    return;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kExpand;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::Collapse(const BrowserAccessibility& node) {
  if (!delegate_) {
    return;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kCollapse;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::ShowContextMenu(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SignalEndOfTest() {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSignalEndOfTest;
  delegate_->AccessibilityPerformAction(action_data);
}

void BrowserAccessibilityManager::Scroll(const BrowserAccessibility& node,
                                         ax::mojom::Action scroll_action) {
  if (!delegate_)
    return;

  switch (scroll_action) {
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Cannot call Scroll with action=" << scroll_action;
  }
  AXActionData action_data;
  action_data.action = scroll_action;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node,
    gfx::Rect subfocus,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) {
  if (!delegate_)
    return;

  base::RecordAction(
      base::UserMetricsAction("Accessibility.NativeApi.ScrollToMakeVisible"));

  base::UmaHistogramBoolean("Accessibility.ScreenReader.ScrollToImage",
                            node.GetRole() == ax::mojom::Role::kImage);

  AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = subfocus;
  action_data.horizontal_scroll_alignment = horizontal_scroll_alignment;
  action_data.vertical_scroll_alignment = vertical_scroll_alignment;
  action_data.scroll_behavior = scroll_behavior;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node,
    gfx::Point point) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = point;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetScrollOffset(
    const BrowserAccessibility& node,
    gfx::Point offset) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point = offset;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetValue(const BrowserAccessibility& node,
                                           const std::string& value) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.target_node_id = node.GetId();
  action_data.action = ax::mojom::Action::kSetValue;
  action_data.value = value;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetSelection(
    const AXActionData& action_data) {
  if (!delegate_)
    return;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetSelection(
    const BrowserAccessibility::AXRange& range) {
  if (!delegate_ || range.IsNull())
    return;

  AXActionData action_data;
  action_data.anchor_node_id = range.anchor()->anchor_id();
  action_data.anchor_offset = range.anchor()->text_offset();
  action_data.focus_node_id = range.focus()->anchor_id();
  action_data.focus_offset = range.focus()->text_offset();
  action_data.action = ax::mojom::Action::kSetSelection;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::StitchChildTree(
    const BrowserAccessibility& node,
    const AXTreeID& child_tree_id) {
  if (!delegate_) {
    return;
  }
  CHECK_NE(child_tree_id, GetTreeID()) << "Circular tree stitching at node:\n"
                                       << node;
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kStitchChildTree;
  action_data.target_tree_id = GetTreeID();
  action_data.target_node_id = node.GetId();
  action_data.child_tree_id = child_tree_id;
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::LoadInlineTextBoxes(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  if (!AXPlatform::GetInstance().GetMode().has_mode(AXMode::kInlineTextBoxes)) {
    return;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kLoadInlineTextBoxes;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::SetAccessibilityFocus(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetAccessibilityFocus;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::ClearAccessibilityFocus(
    const BrowserAccessibility& node) {
  if (!delegate_)
    return;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kClearAccessibilityFocus;
  action_data.target_node_id = node.GetId();
  delegate_->AccessibilityPerformAction(action_data);
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

void BrowserAccessibilityManager::HitTest(const gfx::Point& frame_point,
                                          int request_id) const {
  if (!delegate_)
    return;

  delegate_->AccessibilityHitTest(frame_point, ax::mojom::Event::kHover,
                                  request_id,
                                  /*opt_callback=*/{});
  AXPlatform::GetInstance().NotifyAccessibilityApiUsage();
}

gfx::Rect BrowserAccessibilityManager::GetViewBoundsInScreenCoordinates()
    const {
  AXPlatformTreeManagerDelegate* delegate = GetDelegateFromRootManager();
  if (delegate) {
    gfx::Rect bounds = delegate->AccessibilityGetViewBounds();

    // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
    // The bounds returned by the delegate are always in device-independent
    // pixels (DIPs), meaning physical pixels divided by device scale factor
    // (DSF). However, Blink does not apply DSF when going from physical to
    // screen pixels. In that case, we need to multiply DSF back in to get to
    // Blink's notion of "screen pixels."
    //
    // TODO(vmpstr): This should return physical coordinates always to avoid
    // confusion in the calling code. The calling code should be responsible
    // for converting to whatever space necessary.
    if (device_scale_factor() > 0.0 && device_scale_factor() != 1.0) {
      bounds = ScaleToEnclosingRect(bounds, device_scale_factor());
    }
    return bounds;
  }
  return gfx::Rect();
}

// static
// Next object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::NextInTreeOrder(
    const BrowserAccessibility* object) {
  if (!object)
    return nullptr;

  if (object->PlatformChildCount())
    return object->PlatformGetFirstChild();

  while (object) {
    BrowserAccessibility* sibling = object->PlatformGetNextSibling();
    if (sibling)
      return sibling;

    object = object->PlatformGetParent();
  }

  return nullptr;
}

// static
// Next non-descendant object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::NextNonDescendantInTreeOrder(
    const BrowserAccessibility* object) {
  if (!object)
    return nullptr;

  while (object) {
    BrowserAccessibility* sibling = object->PlatformGetNextSibling();
    if (sibling)
      return sibling;

    object = object->PlatformGetParent();
  }

  return nullptr;
}

// static
// Previous object in tree using depth-first pre-order traversal.
BrowserAccessibility* BrowserAccessibilityManager::PreviousInTreeOrder(
    const BrowserAccessibility* object,
    bool can_wrap_to_last_element) {
  if (!object)
    return nullptr;

  // For android, this needs to be handled carefully. If not, there is a chance
  // of getting into infinite loop.
  if (can_wrap_to_last_element &&
      object->manager()->GetBrowserAccessibilityRoot() == object &&
      object->PlatformChildCount() != 0) {
    return object->PlatformDeepestLastChild();
  }

  BrowserAccessibility* sibling = object->PlatformGetPreviousSibling();
  if (!sibling)
    return object->PlatformGetParent();

  if (sibling->PlatformChildCount())
    return sibling->PlatformDeepestLastChild();

  return sibling;
}

// static
BrowserAccessibility* BrowserAccessibilityManager::PreviousTextOnlyObject(
    const BrowserAccessibility* object) {
  BrowserAccessibility* previous_object = PreviousInTreeOrder(object, false);
  while (previous_object && !previous_object->IsText())
    previous_object = PreviousInTreeOrder(previous_object, false);

  return previous_object;
}

// static
BrowserAccessibility* BrowserAccessibilityManager::NextTextOnlyObject(
    const BrowserAccessibility* object) {
  BrowserAccessibility* next_object = NextInTreeOrder(object);
  while (next_object && !next_object->IsText())
    next_object = NextInTreeOrder(next_object);

  return next_object;
}

// static
bool BrowserAccessibilityManager::FindIndicesInCommonParent(
    const BrowserAccessibility& object1,
    const BrowserAccessibility& object2,
    BrowserAccessibility** common_parent,
    size_t* child_index1,
    size_t* child_index2) {
  DCHECK(common_parent && child_index1 && child_index2);
  auto* ancestor1 = const_cast<BrowserAccessibility*>(&object1);
  auto* ancestor2 = const_cast<BrowserAccessibility*>(&object2);
  do {
    *child_index1 = ancestor1->GetIndexInParent().value_or(0);
    ancestor1 = ancestor1->PlatformGetParent();
  } while (
      ancestor1 &&
      // |BrowserAccessibility::IsAncestorOf| returns true if objects are equal.
      (ancestor1 == ancestor2 || !ancestor2->IsDescendantOf(ancestor1)));

  if (!ancestor1)
    return false;

  do {
    *child_index2 = ancestor2->GetIndexInParent().value();
    ancestor2 = ancestor2->PlatformGetParent();
  } while (ancestor1 != ancestor2);

  *common_parent = ancestor1;
  return true;
}

// static
ax::mojom::TreeOrder BrowserAccessibilityManager::CompareNodes(
    const BrowserAccessibility& object1,
    const BrowserAccessibility& object2) {
  if (&object1 == &object2)
    return ax::mojom::TreeOrder::kEqual;

  BrowserAccessibility* common_parent;
  size_t child_index1;
  size_t child_index2;
  if (FindIndicesInCommonParent(object1, object2, &common_parent, &child_index1,
                                &child_index2)) {
    if (child_index1 < child_index2)
      return ax::mojom::TreeOrder::kBefore;
    if (child_index1 > child_index2)
      return ax::mojom::TreeOrder::kAfter;
  }

  if (object2.IsDescendantOf(&object1))
    return ax::mojom::TreeOrder::kBefore;
  if (object1.IsDescendantOf(&object2))
    return ax::mojom::TreeOrder::kAfter;

  return ax::mojom::TreeOrder::kUndefined;
}

std::vector<const BrowserAccessibility*>
BrowserAccessibilityManager::FindTextOnlyObjectsInRange(
    const BrowserAccessibility& start_object,
    const BrowserAccessibility& end_object) {
  std::vector<const BrowserAccessibility*> text_only_objects;
  size_t child_index1 = 0;
  size_t child_index2 = 0;
  if (&start_object != &end_object) {
    BrowserAccessibility* common_parent;
    if (!FindIndicesInCommonParent(start_object, end_object, &common_parent,
                                   &child_index1, &child_index2)) {
      return text_only_objects;
    }

    DCHECK(common_parent);
    // If the child indices are equal, one object is a descendant of the other.
    DCHECK(child_index1 != child_index2 ||
           start_object.IsDescendantOf(&end_object) ||
           end_object.IsDescendantOf(&start_object));
  }

  const BrowserAccessibility* start_text_object = nullptr;
  const BrowserAccessibility* end_text_object = nullptr;
  if (&start_object == &end_object && start_object.IsAtomicTextField()) {
    // We need to get to the shadow DOM that is inside the text control in order
    // to find the text-only objects.
    if (!start_object.InternalChildCount())
      return text_only_objects;
    start_text_object = start_object.InternalGetFirstChild();
    end_text_object = start_object.InternalGetLastChild();
  } else if (child_index1 <= child_index2 ||
             end_object.IsDescendantOf(&start_object)) {
    start_text_object = &start_object;
    end_text_object = &end_object;
  } else if (child_index1 > child_index2 ||
             start_object.IsDescendantOf(&end_object)) {
    start_text_object = &end_object;
    end_text_object = &start_object;
  }

  // Pre-order traversal might leave some text-only objects behind if we don't
  // start from the deepest children of the end object.
  if (!end_text_object->IsLeaf())
    end_text_object = end_text_object->PlatformDeepestLastChild();

  if (!start_text_object->IsText())
    start_text_object = NextTextOnlyObject(start_text_object);
  if (!end_text_object->IsText())
    end_text_object = PreviousTextOnlyObject(end_text_object);

  if (!start_text_object || !end_text_object)
    return text_only_objects;

  while (start_text_object && start_text_object != end_text_object) {
    text_only_objects.push_back(start_text_object);
    start_text_object = NextTextOnlyObject(start_text_object);
  }
  text_only_objects.push_back(end_text_object);

  return text_only_objects;
}

// static
std::u16string BrowserAccessibilityManager::GetTextForRange(
    const BrowserAccessibility& start_object,
    const BrowserAccessibility& end_object) {
  return GetTextForRange(start_object, 0, end_object,
                         end_object.GetTextContentUTF16().length());
}

// static
std::u16string BrowserAccessibilityManager::GetTextForRange(
    const BrowserAccessibility& start_object,
    int start_offset,
    const BrowserAccessibility& end_object,
    int end_offset) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);

  if (&start_object == &end_object && start_object.IsAtomicTextField()) {
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    if (start_offset >=
            static_cast<int>(start_object.GetTextContentUTF16().length()) ||
        end_offset >
            static_cast<int>(start_object.GetTextContentUTF16().length())) {
      return std::u16string();
    }

    return start_object.GetTextContentUTF16().substr(start_offset,
                                                     end_offset - start_offset);
  }

  std::vector<const BrowserAccessibility*> text_only_objects =
      FindTextOnlyObjectsInRange(start_object, end_object);
  if (text_only_objects.empty())
    return std::u16string();

  if (text_only_objects.size() == 1) {
    // Be a little permissive with the start and end offsets.
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    const BrowserAccessibility* text_object = text_only_objects[0];
    if (start_offset <
            static_cast<int>(text_object->GetTextContentUTF16().length()) &&
        end_offset <=
            static_cast<int>(text_object->GetTextContentUTF16().length())) {
      return text_object->GetTextContentUTF16().substr(
          start_offset, end_offset - start_offset);
    }
    return text_object->GetTextContentUTF16();
  }

  std::u16string text;
  const BrowserAccessibility* start_text_object = text_only_objects[0];
  // Figure out if the start and end positions have been reversed.
  const BrowserAccessibility* first_object = &start_object;
  if (!first_object->IsText())
    first_object = NextTextOnlyObject(first_object);
  if (!first_object || first_object != start_text_object)
    std::swap(start_offset, end_offset);

  if (start_offset <
      static_cast<int>(start_text_object->GetTextContentUTF16().length())) {
    text += start_text_object->GetTextContentUTF16().substr(start_offset);
  } else {
    text += start_text_object->GetTextContentUTF16();
  }

  for (size_t i = 1; i < text_only_objects.size() - 1; ++i) {
    text += text_only_objects[i]->GetTextContentUTF16();
  }

  const BrowserAccessibility* end_text_object = text_only_objects.back();
  if (end_offset <=
      static_cast<int>(end_text_object->GetTextContentUTF16().length())) {
    text += end_text_object->GetTextContentUTF16().substr(0, end_offset);
  } else {
    text += end_text_object->GetTextContentUTF16();
  }

  return text;
}

// static
gfx::Rect BrowserAccessibilityManager::GetRootFrameInnerTextRangeBoundsRect(
    const BrowserAccessibility& start_object,
    int start_offset,
    const BrowserAccessibility& end_object,
    int end_offset) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, 0);

  if (&start_object == &end_object && start_object.IsAtomicTextField()) {
    if (start_offset > end_offset)
      std::swap(start_offset, end_offset);

    if (start_offset >=
            static_cast<int>(start_object.GetTextContentUTF16().length()) ||
        end_offset >
            static_cast<int>(start_object.GetTextContentUTF16().length())) {
      return gfx::Rect();
    }

    return start_object.GetUnclippedRootFrameInnerTextRangeBoundsRect(
        start_offset, end_offset);
  }

  gfx::Rect result;
  const BrowserAccessibility* first = &start_object;
  const BrowserAccessibility* last = &end_object;

  switch (CompareNodes(*first, *last)) {
    case ax::mojom::TreeOrder::kBefore:
    case ax::mojom::TreeOrder::kEqual:
      break;
    case ax::mojom::TreeOrder::kAfter:
      std::swap(first, last);
      std::swap(start_offset, end_offset);
      break;
    default:
      return gfx::Rect();
  }

  const BrowserAccessibility* current = first;
  do {
    if (current->IsText()) {
      int len = static_cast<int>(current->GetTextContentUTF16().size());
      int start_char_index = 0;
      int end_char_index = len;
      if (current == first)
        start_char_index = start_offset;
      if (current == last)
        end_char_index = end_offset;
      result.Union(current->GetUnclippedRootFrameInnerTextRangeBoundsRect(
          start_char_index, end_char_index));
    } else {
      result.Union(current->GetClippedRootFrameBoundsRect());
    }

    if (current == last)
      break;

    current = NextInTreeOrder(current);
  } while (current);

  return result;
}

void BrowserAccessibilityManager::OnNodeCreated(AXTree* tree, AXNode* node) {
  DCHECK(node);
  DCHECK(node->IsDataValid());
  DCHECK(tree->GetFromId(node->id()) || node->IsGenerated())
      << "Node must be in AXTree's map, unless it's an ExtraMacNode.";

  id_wrapper_map_[node->id()] = CreateBrowserAccessibility(node);

  if (node->HasIntAttribute(ax::mojom::IntAttribute::kPopupForId)) {
    popup_root_ids_.insert(node->id());
  }
}

void BrowserAccessibilityManager::OnNodeReparented(AXTree* tree, AXNode* node) {
  auto iter = id_wrapper_map_.find(CHECK_DEREF(node).id());
  // TODO(crbug.com/40833630): This condition should never occur.
  // Identify why we are entering this code path and fix the root cause, then
  // remove the early return. Will need to update
  // BrowserAccessibilityManagerTest.TestOnNodeReparented, which purposely
  // triggers this condition.
  SANITIZER_CHECK(iter != id_wrapper_map_.end())
      << "Missing BrowserAccessibility* for node: " << *node
      << "\nTree: " << tree->ToString(/*verbose*/ false);
  if (iter == id_wrapper_map_.end()) {
    return;
  }
  CHECK_DEREF(iter->second.get()).SetNode(*node);
}

void BrowserAccessibilityManager::OnAtomicUpdateStarting(
    AXTree* tree,
    const std::set<AXNodeID>& deleted_node_ids,
    const std::set<AXNodeID>& reparented_node_ids) {
  for (const auto& id : deleted_node_ids) {
    id_wrapper_map_.erase(id);
    popup_root_ids_.erase(id);
    node_id_delegate_->OnAXNodeDeleted(id);
  }

  for (const auto& id : reparented_node_ids) {
    if (auto iter = id_wrapper_map_.find(id); iter != id_wrapper_map_.end()) {
      CHECK_DEREF(iter->second.get()).reset_node();
    }
  }
}

void BrowserAccessibilityManager::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<AXTreeObserver::Change>& changes) {
  AXTreeManager::OnAtomicUpdateFinished(tree, root_changed, changes);
  // Calls OnDataChanged on newly created, reparented or changed nodes.
  for (const auto& change : changes) {
    AXNode* node = change.node;
    BrowserAccessibility* wrapper = GetFromAXNode(node);
    if (wrapper)
      wrapper->OnDataChanged();
  }
}

AXNode* BrowserAccessibilityManager::GetNode(const AXNodeID node_id) const {
  // This does not use ax_tree()->FromID(), because that uses a different map
  // that does not contain extra mac nodes from AXTableInfo.
  BrowserAccessibility* browser_accessibility = GetFromID(node_id);
  return browser_accessibility ? browser_accessibility->node() : nullptr;
}

AXPlatformNode* BrowserAccessibilityManager::GetPlatformNodeFromTree(
    const AXNodeID node_id) const {
  BrowserAccessibility* wrapper = GetFromID(node_id);
  if (wrapper)
    return wrapper->GetAXPlatformNode();
  return nullptr;
}

AXPlatformNode* BrowserAccessibilityManager::GetPlatformNodeFromTree(
    const AXNode& node) const {
  return GetPlatformNodeFromTree(node.id());
}

AXPlatformNodeDelegate* BrowserAccessibilityManager::RootDelegate() const {
  return GetBrowserAccessibilityRoot();
}

BrowserAccessibilityManager*
BrowserAccessibilityManager::GetManagerForRootFrame() const {
  if (IsRootFrameManager())
    return const_cast<BrowserAccessibilityManager*>(this);

  BrowserAccessibilityManager* parent_manager =
      static_cast<BrowserAccessibilityManager*>(GetParentManager());
  if (!parent_manager) {
    // This can occur when the child frame has an embedding token, but the
    // parent element (e.g. <iframe>) does not yet know about the child.
    // Attempting to change this to a DCHECK() will currently cause a number of
    // tests to fail. Ideally, we would not need this if Blink always serialized
    // the embedding token in the child tree owning element first, before
    // serializing the child tree.
    return nullptr;
  }

  return parent_manager->GetManagerForRootFrame();
}

AXTreeManager* BrowserAccessibilityManager::GetParentManager() const {
  // `AXTreeManager::GetParentManager` can still return null if the parent frame
  // has not yet been serialized. We can't prevent a child frame from
  // serializing before the parent frame does, because the child frame does not
  // have access to the parent in the case of remote frames, aka Out-Of-Process
  // Iframes, aka OOPIFs.
  AXTreeManager* parent = AXTreeManager::GetParentManager();
  if (!parent)
    return nullptr;

  DCHECK(!IsRootFrameManager());

  return parent;
}

AXPlatformTreeManagerDelegate*
BrowserAccessibilityManager::GetDelegateFromRootManager() const {
  BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
  if (root_manager)
    return root_manager->delegate();
  return nullptr;
}

bool BrowserAccessibilityManager::IsRootFrameManager() const {
  // delegate_ can be null in unit tests.
  if (!delegate_)
    return GetTreeData().parent_tree_id == AXTreeIDUnknown();

  bool is_root_tree = delegate_->AccessibilityIsRootFrame();
  DCHECK(!is_root_tree || GetParentTreeID() == AXTreeIDUnknown())
      << "Root tree has parent tree id of: " << GetParentTreeID();
  return is_root_tree;
}

AXTreeUpdate BrowserAccessibilityManager::SnapshotAXTreeForTesting() {
  std::unique_ptr<AXTreeSource<const AXNode*, AXTreeData*, AXNodeData>>
      tree_source(ax_serializable_tree()->CreateTreeSource());
  AXTreeSerializer<const AXNode*, std::vector<const AXNode*>, AXTreeUpdate*,
                   AXTreeData*, AXNodeData>
      serializer(tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(GetRoot(), &update);
  return update;
}

void BrowserAccessibilityManager::UseCustomDeviceScaleFactorForTesting(
    float device_scale_factor) {
  use_custom_device_scale_factor_for_testing_ = true;
  device_scale_factor_ = device_scale_factor;
}

BrowserAccessibility* BrowserAccessibilityManager::CachingAsyncHitTest(
    const gfx::Point& physical_pixel_point) const {
  // TODO(crbug.com/40679532): By starting the hit test on the root frame,
  // it allows for the possibility that we don't return a descendant as the
  // hit test result, but AXPlatformNodeDelegate says that it's only supposed
  // to return a descendant, so this isn't correctly fulfilling the contract.
  // Unchecked it can even lead to an infinite loop.
  BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
  if (root_manager && root_manager != this)
    return root_manager->CachingAsyncHitTest(physical_pixel_point);

  gfx::Point blink_screen_point = physical_pixel_point;

  gfx::Rect screen_view_bounds = GetViewBoundsInScreenCoordinates();

  if (delegate_) {
    // Transform from screen to viewport to frame coordinates to pass to Blink.
    // Note that page scale (pinch zoom) is independent of device scale factor
    // (display DPI). Only the latter is affected by UseZoomForDSF.
    // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
    gfx::Point viewport_point =
        blink_screen_point - screen_view_bounds.OffsetFromOrigin();
    gfx::Point frame_point =
        gfx::ScaleToRoundedPoint(viewport_point, 1.0f / page_scale_factor_);

    // This triggers an asynchronous request to compute the true object that's
    // under the point.
    HitTest(frame_point, /*request_id=*/0);

    // Unfortunately we still have to return an answer synchronously because
    // the APIs were designed that way. The best case scenario is that the
    // screen point is within the bounds of the last result we got from a
    // call to AccessibilityHitTest - in that case, we can return that object!
    if (last_hover_bounds_.Contains(blink_screen_point)) {
      BrowserAccessibilityManager* manager =
          BrowserAccessibilityManager::FromID(last_hover_ax_tree_id_);
      if (manager) {
        BrowserAccessibility* node = manager->GetFromID(last_hover_node_id_);
        if (node)
          return node;
      }
    }
  }

  // If that test failed we have to fall back on searching the accessibility
  // tree locally for the best bounding box match. This is generally right
  // for simple pages but wrong in cases of z-index, overflow, and other
  // more complicated layouts. The hope is that if the user is moving the
  // mouse, this fallback will only be used transiently, and the asynchronous
  // result will be used for the next call.
  return ApproximateHitTest(blink_screen_point);
}

BrowserAccessibility* BrowserAccessibilityManager::ApproximateHitTest(
    const gfx::Point& blink_screen_point) const {
  if (cached_node_rtree_)
    return AXTreeHitTest(blink_screen_point);

  return GetBrowserAccessibilityRoot()->ApproximateHitTest(blink_screen_point);
}

void BrowserAccessibilityManager::DetachFromParentManager() {
  connected_to_parent_tree_node_ = false;
}

void BrowserAccessibilityManager::BuildAXTreeHitTestCache() {
  auto* root = GetBrowserAccessibilityRoot();
  if (!root)
    return;

  std::vector<const BrowserAccessibility*> storage;
  BuildAXTreeHitTestCacheInternal(root, &storage);
  // Use AXNodeID for this as nodes are unchanging with this cache.
  cached_node_rtree_ = std::make_unique<cc::RTree<AXNodeID>>();
  cached_node_rtree_->Build(
      storage.size(),
      [&storage](size_t index) {
        return storage[index]->GetUnclippedRootFrameBoundsRect();
      },
      [&storage](size_t index) { return storage[index]->GetId(); });
}

void BrowserAccessibilityManager::BuildAXTreeHitTestCacheInternal(
    const BrowserAccessibility* node,
    std::vector<const BrowserAccessibility*>* storage) {
  // Based on behavior in ApproximateHitTest() and node ordering in Blink:
  // Generated backwards so that in the absence of any other information, we
  // assume the object that occurs later in the tree is on top of one that comes
  // before it.
  auto range = node->PlatformChildren();
  for (const auto& child : base::Reversed(range)) {
    // Skip table columns because cells are only contained in rows,
    // not columns.
    if (child.GetRole() == ax::mojom::Role::kColumn)
      continue;

    BuildAXTreeHitTestCacheInternal(&child, storage);
  }

  storage->push_back(node);
}

BrowserAccessibility* BrowserAccessibilityManager::AXTreeHitTest(
    const gfx::Point& blink_screen_point) const {
  // TODO(crbug.com/40211255): assert that this gets called on a valid node.
  // This should usually be the root node except for Paint Preview.
  DCHECK(cached_node_rtree_);

  std::vector<AXNodeID> results;
  std::vector<gfx::Rect> rects;
  cached_node_rtree_->Search(
      gfx::Rect(blink_screen_point.x(), blink_screen_point.y(), /*width=*/1,
                /*height=*/1),
      &results, &rects);

  if (results.empty())
    return nullptr;

  // Find the tightest enclosing rect. Work backwards as leaf nodes come
  // last and should be preferred.
  auto rit = std::min_element(rects.rbegin(), rects.rend(),
                              [](const gfx::Rect& a, const gfx::Rect& b) {
                                return a.size().Area64() < b.size().Area64();
                              });
  return GetFromID(results[std::distance(rects.begin(), rit.base()) - 1]);
}

void BrowserAccessibilityManager::CacheHitTestResult(
    BrowserAccessibility* hit_test_result) const {
  // Walk up to the highest ancestor that's a leaf node; we don't want to
  // return a node that's hidden from the tree.
  hit_test_result = hit_test_result->PlatformGetLowestPlatformAncestor();

  last_hover_ax_tree_id_ = hit_test_result->manager()->GetTreeID();
  last_hover_node_id_ = hit_test_result->GetId();
  last_hover_bounds_ = hit_test_result->GetClippedScreenBoundsRect();
}

void BrowserAccessibilityManager::SetPageScaleFactor(float page_scale_factor) {
  page_scale_factor_ = page_scale_factor;
}

float BrowserAccessibilityManager::GetPageScaleFactor() const {
  return page_scale_factor_;
}

void BrowserAccessibilityManager::CollectChangedNodesAndParentsForAtomicUpdate(
    AXTree* tree,
    const std::vector<AXTreeObserver::Change>& changes,
    std::set<AXPlatformNode*>* nodes_needing_update) {
  // The nodes that need to be updated are all of the nodes that were changed,
  // plus some parents.
  for (const auto& change : changes) {
    const AXNode* changed_node = change.node;
    DCHECK(changed_node);

    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj)
      nodes_needing_update->insert(obj->GetAXPlatformNode());

    const AXNode* parent = changed_node->GetUnignoredParent();
    if (!parent)
      continue;

    // Update changed nodes' parents, including their hypertext:
    // Any child that changes, whether text or not, can affect the parent's
    // hypertext. Hypertext uses embedded object characters to represent
    // child objects, and the AXHyperText caches relevant object at
    // each embedded object character offset.
    if (changed_node->data().role != ax::mojom::Role::kInlineTextBox) {
      BrowserAccessibility* parent_obj = GetFromAXNode(parent);
      if (parent_obj) {
        nodes_needing_update->insert(parent_obj->GetAXPlatformNode());
      }
    }

    // When a node is editable, update the editable root too.
    if (!changed_node->HasState(ax::mojom::State::kEditable))
      continue;
    const AXNode* editable_root = changed_node;
    while (editable_root->parent() &&
           editable_root->parent()->HasState(ax::mojom::State::kEditable)) {
      editable_root = editable_root->parent();
    }

    BrowserAccessibility* editable_root_obj = GetFromAXNode(editable_root);
    if (editable_root_obj)
      nodes_needing_update->insert(editable_root_obj->GetAXPlatformNode());
  }
}

bool BrowserAccessibilityManager::ShouldFireEventForNode(
    BrowserAccessibility* node) const {
  node = RetargetBrowserAccessibilityForEvents(
      node, RetargetEventType::RetargetEventTypeGenerated);
  if (!node || !node->CanFireEvents())
    return false;

  // If the root delegate isn't the main-frame, this may be a new frame that
  // hasn't yet been swapped in or added to the frame tree. Suppress firing
  // events until then.
  AXPlatformTreeManagerDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate)
    return false;
  if (!root_delegate->AccessibilityIsRootFrame())
    return false;

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return false;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to the platform.
  if (node->GetRole() == ax::mojom::Role::kInlineTextBox)
    return false;

  return true;
}

std::unique_ptr<BrowserAccessibility>
BrowserAccessibilityManager::CreateBrowserAccessibility(AXNode* node) {
#if !BUILDFLAG(IS_ANDROID)
  return BrowserAccessibility::Create(this, node);
#else
  NOTREACHED();
#endif
}

BrowserAccessibility*
BrowserAccessibilityManager::RetargetBrowserAccessibilityForEvents(
    BrowserAccessibility* node,
    RetargetEventType event_type) const {
  if (!node) {
    // TODO(accessibility): |node| should never be null, however for
    // reasons that are not yet clear, it is sometimes null.
    // See https://crbug.com/1350627, https://crbug.com/1362266 and
    // https://crbug.com/1362321.
    // ClusterFuzz was able to come up with a reliably-reproducible test case
    // which can be seen in https://crbug.com/1362230. This needs to be
    // investigated further.
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  return GetFromAXNode(RetargetForEvents(node->node(), event_type));
}

AXPlatformNodeId BrowserAccessibilityManager::GetNodeUniqueId(
    const BrowserAccessibility* node) {
  return node_id_delegate_->GetOrCreateAXNodeUniqueId(node->node()->id());
}

float BrowserAccessibilityManager::device_scale_factor() const {
  return device_scale_factor_;
}

void BrowserAccessibilityManager::UpdateDeviceScaleFactor() {
  if (delegate_)
    device_scale_factor_ = delegate_->AccessibilityGetDeviceScaleFactor();
}

}  // namespace ui
