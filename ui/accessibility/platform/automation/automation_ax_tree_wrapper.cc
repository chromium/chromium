// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"

#include <map>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"

namespace ui {

// Multiroot tree lookup.
// Represents an app node.
struct AppNodeInfo {
  AXTreeID tree_id;
  int32_t node_id;
};

// These maps support moving from a node to a descendant tree node via an app id
// (and vice versa).
std::map<std::string, std::pair<AXTreeID, int32_t>>&
GetAppIDToParentTreeNodeMap() {
  static base::NoDestructor<std::map<std::string, std::pair<AXTreeID, int32_t>>>
      app_id_to_tree_node_map;
  return *app_id_to_tree_node_map;
}

std::map<std::string, std::vector<AppNodeInfo>>& GetAppIDToTreeNodeMap() {
  static base::NoDestructor<std::map<std::string, std::vector<AppNodeInfo>>>
      app_id_to_tree_node_map;
  return *app_id_to_tree_node_map;
}

AutomationAXTreeWrapper::AutomationAXTreeWrapper(
    AutomationTreeManagerOwner* owner)
    : AXTreeManager(std::make_unique<AXTree>()), owner_(owner) {}

AutomationAXTreeWrapper::~AutomationAXTreeWrapper() {
  // Code paths, when not exiting gracefully, may leave a reference to an
  // invalid pointer in the map (which is static). One known case is tests that
  // create a tree with a child tree and do not destroy the objects through
  // automation standard ways (resetting automation, disabling an individual
  // tree).
  std::map<AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& child_tree_ids = ax_tree_->GetAllChildTreeIds();
  std::erase_if(child_tree_id_reverse_map, [&child_tree_ids](auto& pair) {
    return child_tree_ids.count(pair.first);
  });
}

// static
AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentOfTreeId(
    AXTreeID tree_id) {
  std::map<AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& iter = child_tree_id_reverse_map.find(tree_id);
  if (iter != child_tree_id_reverse_map.end())
    return iter->second;

  return nullptr;
}

bool AutomationAXTreeWrapper::OnAccessibilityEvents(
    const AXTreeID& tree_id,
    const std::vector<AXTreeUpdate>& updates,
    const std::vector<AXEvent>& events,
    gfx::Point mouse_location) {
  TRACE_EVENT0("accessibility",
               "AutomationAXTreeWrapper::OnAccessibilityEvents");

  std::optional<gfx::Rect> previous_accessibility_focused_global_bounds =
      owner_->GetAccessibilityFocusedLocation();

  std::map<AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& child_tree_ids = ax_tree_->GetAllChildTreeIds();

  // Invalidate any reverse child tree id mappings. Note that it is possible
  // there are no entries in this map for a given child tree to |this|, if this
  // is the first event from |this| tree or if |this| was destroyed and (and
  // then reset).
  std::erase_if(child_tree_id_reverse_map, [&child_tree_ids](auto& pair) {
    return child_tree_ids.count(pair.first);
  });

  // Unserialize all incoming data.
  for (const auto& update : updates) {
    deleted_node_ids_.clear();
    did_send_tree_change_during_unserialization_ = false;

    if (!ax_tree_->Unserialize(update)) {
      static crash_reporter::CrashKeyString<4> crash_key(
          "ax-tree-wrapper-unserialize-failed");
      crash_key.Set("yes");
      event_generator_.ClearEvents();
      return false;
    }

      owner_->SendNodesRemovedEvent(ax_tree(), deleted_node_ids_);

      if (update.nodes.size() && did_send_tree_change_during_unserialization_) {
        owner_->SendTreeChangeEvent(ax::mojom::Mutation::kSubtreeUpdateEnd,
                                    ax_tree(), ax_tree_->root());
      }
  }

  // Refresh child tree id  mappings.
  for (const AXTreeID& child_tree_id : ax_tree_->GetAllChildTreeIds()) {
    DCHECK(!base::Contains(child_tree_id_reverse_map, child_tree_id));
    child_tree_id_reverse_map.insert(std::make_pair(child_tree_id, this));
  }

  // Perform language detection first thing if we see a load complete event.
  // We have to run *before* we send the load complete event to javascript
  // otherwise code which runs immediately on load complete will not be able
  // to see the results of language detection.
  //
  // Currently language detection only runs once for initial load complete, any
  // content loaded after this will not have language detection performed for
  // it.
  for (const auto& event : events) {
    if (event.event_type == ax::mojom::Event::kLoadComplete) {
      ax_tree_->language_detection_manager->DetectLanguages();
      ax_tree_->language_detection_manager->LabelLanguages();

      // After initial language detection, enable language detection for future
      // content updates in order to support dynamic content changes.
      //
      // If the LanguageDetectionDynamic feature flag is not enabled then this
      // is a no-op.
      ax_tree_->language_detection_manager->RegisterLanguageDetectionObserver();

      break;
    }
  }

  // Send all blur and focus events first.
  owner_->MaybeSendFocusAndBlur(this, tree_id, updates, events, mouse_location);

  // Send auto-generated AXEventGenerator events.
  for (const auto& targeted_event : event_generator_) {
    if (ShouldIgnoreGeneratedEventForAutomation(
            targeted_event.event_params->event)) {
      continue;
    }
    AXEvent generated_event;
    generated_event.id = targeted_event.node_id;
    generated_event.event_from = targeted_event.event_params->event_from;
    generated_event.event_from_action =
        targeted_event.event_params->event_from_action;
    generated_event.event_intents = targeted_event.event_params->event_intents;
    owner_->SendAutomationEvent(tree_id, mouse_location, generated_event,
                                targeted_event.event_params->event);
  }
  event_generator_.ClearEvents();

  for (const auto& event : events) {
    if (event.event_type == ax::mojom::Event::kFocus ||
        event.event_type == ax::mojom::Event::kBlur)
      continue;

    // Send some events directly.
    if (!ShouldIgnoreAXEventForAutomation(event.event_type)) {
      owner_->SendAutomationEvent(tree_id, mouse_location, event);
    }
  }

  if (previous_accessibility_focused_global_bounds.has_value() &&
      previous_accessibility_focused_global_bounds !=
          owner_->GetAccessibilityFocusedLocation()) {
    owner_->SendAccessibilityFocusedLocationChange(mouse_location);
  }

  return true;
}

bool AutomationAXTreeWrapper::IsDesktopTree() const {
  return ax_tree_->root()
             ? ax_tree_->root()->GetRole() == ax::mojom::Role::kDesktop
             : false;
}

bool AutomationAXTreeWrapper::HasDeviceScaleFactor() const {
  return ax_tree_->root() ?
                          // These are views-backed trees.
             ax_tree_->root()->GetRole() != ax::mojom::Role::kDesktop &&
                 ax_tree_->root()->GetRole() != ax::mojom::Role::kClient
                          : true;
}

bool AutomationAXTreeWrapper::IsInFocusChain(int32_t node_id) {
  if (ax_tree_->data().focus_id != node_id)
    return false;

  if (IsDesktopTree())
    return true;

  AutomationAXTreeWrapper* descendant_tree = this;
  AXTreeID descendant_tree_id = GetTreeID();
  AutomationAXTreeWrapper* ancestor_tree = descendant_tree;
  bool found = true;
  while ((ancestor_tree = ancestor_tree->GetParentTree())) {
    int32_t ancestor_tree_focus_id = ancestor_tree->ax_tree()->data().focus_id;
    AXNode* ancestor_tree_focused_node =
        ancestor_tree->ax_tree()->GetFromId(ancestor_tree_focus_id);
    if (!ancestor_tree_focused_node)
      return false;

    if (ancestor_tree_focused_node->HasStringAttribute(
            ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
      // |ancestor_tree_focused_node| points to a tree with multiple roots as
      // its child tree node. Ensure the node points back to
      // |ancestor_tree_focused_node| as its parent.
      AXNode* parent_node = descendant_tree->GetParentTreeNodeForAppID(
          ancestor_tree_focused_node->GetStringAttribute(
              ax::mojom::StringAttribute::kChildTreeNodeAppId),
          owner_);
      if (parent_node != ancestor_tree_focused_node)
        return false;
    } else if (AXTreeID::FromString(
                   ancestor_tree_focused_node->GetStringAttribute(
                       ax::mojom::StringAttribute::kChildTreeId)) !=
                   descendant_tree_id &&
               ancestor_tree->ax_tree()->data().focused_tree_id !=
                   descendant_tree_id) {
      // Surprisingly, an ancestor frame can "skip" a child frame to point to a
      // descendant granchild, so we have to scan upwards.
      found = false;
      continue;
    }

    found = true;

    if (ancestor_tree->IsDesktopTree())
      return true;

    descendant_tree_id = ancestor_tree->GetTreeID();
    descendant_tree = ancestor_tree;
  }

  // We can end up here if the tree is detached from any desktop.  This can
  // occur in tabs-only mode. This is also the codepath for frames with inner
  // focus, but which are not focused by ancestor frames.
  return found;
}

AXSelection AutomationAXTreeWrapper::GetUnignoredSelection() {
  return ax_tree_->GetUnignoredSelection();
}

AXNode* AutomationAXTreeWrapper::GetUnignoredNodeFromId(int32_t id) {
  AXNode* node = ax_tree_->GetFromId(id);
  return (node && !node->IsIgnored()) ? node : nullptr;
}

void AutomationAXTreeWrapper::SetAccessibilityFocus(int32_t node_id) {
  accessibility_focused_id_ = node_id;
}

AXNode* AutomationAXTreeWrapper::GetAccessibilityFocusedNode() {
  return accessibility_focused_id_ == kInvalidAXNodeID
             ? nullptr
             : ax_tree_->GetFromId(accessibility_focused_id_);
}

AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTree() {
  // Explicit parent tree from this tree's data.
  auto* ret = GetParentOfTreeId(ax_tree_->data().tree_id);

  // If this tree has multiple roots, and no explicit parent tree, fallback to
  // any node with a parent tree node app id to find a parent tree.
  return ret ? ret : GetParentTreeFromAnyAppID();
}

AutomationAXTreeWrapper*
AutomationAXTreeWrapper::GetTreeWrapperWithUnignoredRoot() {
  // The desktop is always unignored.
  if (IsDesktopTree())
    return this;

  // Keep following these parent node id links upwards, since we want to ignore
  // these roots for the api in js.
  AutomationAXTreeWrapper* current = this;
  AutomationAXTreeWrapper* parent = this;
  while ((parent = current->GetParentTreeFromAnyAppID()))
    current = parent;

  return current;
}

AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTreeFromAnyAppID() {
  for (const std::string& app_id : all_tree_node_app_ids_) {
    auto* wrapper = GetParentTreeWrapperForAppID(app_id, owner_);
    if (wrapper)
      return wrapper;
  }

  return nullptr;
}

void AutomationAXTreeWrapper::EventListenerAdded(
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
    AXNode* node) {
  node_id_to_events_[node->id()].insert(event_type);

  event_generator_.RegisterEventOnNode(std::get<1>(event_type), node->id());
}

void AutomationAXTreeWrapper::EventListenerRemoved(
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
    AXNode* node) {
  auto it = node_id_to_events_.find(node->id());
  if (it != node_id_to_events_.end()) {
    it->second.erase(event_type);
    if (it->second.empty())
      node_id_to_events_.erase(it);
  }

  event_generator_.UnregisterEventOnNode(std::get<1>(event_type), node->id());
}

bool AutomationAXTreeWrapper::HasEventListener(
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
    AXNode* node) {
  auto it = node_id_to_events_.find(node->id());
  if (it == node_id_to_events_.end())
    return false;

  return it->second.count(event_type);
}

size_t AutomationAXTreeWrapper::EventListenerCount() const {
  return node_id_to_events_.size();
}

// static
std::map<AXTreeID, AutomationAXTreeWrapper*>&
AutomationAXTreeWrapper::GetChildTreeIDReverseMap() {
  static base::NoDestructor<std::map<AXTreeID, AutomationAXTreeWrapper*>>
      child_tree_id_reverse_map;
  return *child_tree_id_reverse_map;
}

// static
AXNode* AutomationAXTreeWrapper::GetParentTreeNodeForAppID(
    const std::string& app_id,
    const AutomationTreeManagerOwner* owner) {
  auto& map = GetAppIDToParentTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return nullptr;

  AutomationAXTreeWrapper* wrapper =
      owner->GetAutomationAXTreeWrapperFromTreeID(it->second.first);
  if (!wrapper)
    return nullptr;

  return wrapper->ax_tree()->GetFromId(it->second.second);
}

// static
AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTreeWrapperForAppID(
    const std::string& app_id,
    const AutomationTreeManagerOwner* owner) {
  auto& map = GetAppIDToParentTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return nullptr;

  return owner->GetAutomationAXTreeWrapperFromTreeID(it->second.first);
}

// static
std::vector<AXNode*> AutomationAXTreeWrapper::GetChildTreeNodesForAppID(
    const std::string& app_id,
    const AutomationTreeManagerOwner* owner) {
  auto& map = GetAppIDToTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return std::vector<AXNode*>();

  std::vector<AXNode*> nodes;
  for (const AppNodeInfo& app_node_info : it->second) {
    AutomationAXTreeWrapper* wrapper =
        owner->GetAutomationAXTreeWrapperFromTreeID(app_node_info.tree_id);
    if (!wrapper)
      continue;

    AXNode* node = wrapper->ax_tree()->GetFromId(app_node_info.node_id);
    // TODO(b:269669313): We don't expect this to ever be null, however this
    // case arises occasionally in the wild and consistently in Dictation C++
    // tests running on Lacros.
    if (node != nullptr) {
      nodes.push_back(node);
    }
  }

  return nodes;
}

void AutomationAXTreeWrapper::OnNodeDataChanged(
    AXTree* tree,
    const AXNodeData& old_node_data,
    const AXNodeData& new_node_data) {
  if (old_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName) !=
      new_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName))
    text_changed_node_ids_.push_back(new_node_data.id);
}

void AutomationAXTreeWrapper::OnStringAttributeChanged(
    AXTree* tree,
    AXNode* node,
    ax::mojom::StringAttribute attr,
    const std::string& old_value,
    const std::string& new_value) {
  if (attr == ax::mojom::StringAttribute::kChildTreeNodeAppId) {
    if (new_value.empty()) {
      GetAppIDToParentTreeNodeMap().erase(old_value);
    } else {
      GetAppIDToParentTreeNodeMap()[new_value] = {tree->GetAXTreeID(),
                                                  node->data().id};
    }
  }

  if (attr == ax::mojom::StringAttribute::kAppId) {
    if (new_value.empty()) {
      auto it = GetAppIDToTreeNodeMap().find(old_value);
      if (it != GetAppIDToTreeNodeMap().end()) {
        std::erase_if(it->second, [node](const AppNodeInfo& app_node_info) {
          return app_node_info.node_id == node->id();
        });
        if (it->second.empty()) {
          GetAppIDToTreeNodeMap().erase(old_value);
          all_tree_node_app_ids_.erase(old_value);
        }
      }
    } else {
      GetAppIDToTreeNodeMap()[new_value].push_back(
          {tree->GetAXTreeID(), node->data().id});
      all_tree_node_app_ids_.insert(new_value);
    }
  }
}

void AutomationAXTreeWrapper::OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {
  did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
      ax::mojom::Mutation::kNodeRemoved, tree, node);
  deleted_node_ids_.push_back(node->id());
  node_id_to_events_.erase(node->id());

  if (node->HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
    GetAppIDToParentTreeNodeMap().erase(node->GetStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId));
  }

  if (node->HasStringAttribute(ax::mojom::StringAttribute::kAppId)) {
    const std::string& app_id =
        node->GetStringAttribute(ax::mojom::StringAttribute::kAppId);
    auto it = GetAppIDToTreeNodeMap().find(app_id);
    if (it != GetAppIDToTreeNodeMap().end()) {
      std::erase_if(it->second, [node](const AppNodeInfo& app_node_info) {
        return app_node_info.node_id == node->id();
      });

      if (it->second.empty()) {
        GetAppIDToTreeNodeMap().erase(app_id);
        all_tree_node_app_ids_.erase(app_id);
      }
    }
  }
}

void AutomationAXTreeWrapper::OnNodeCreated(AXTree* tree, AXNode* node) {
  if (node->HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
    GetAppIDToParentTreeNodeMap()[node->GetStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId)] = {
        node->tree()->GetAXTreeID(), node->id()};
  }

  if (node->HasStringAttribute(ax::mojom::StringAttribute::kAppId)) {
    const std::string& app_id =
        node->GetStringAttribute(ax::mojom::StringAttribute::kAppId);
    GetAppIDToTreeNodeMap()[app_id].push_back(
        {node->tree()->GetAXTreeID(), node->id()});
    all_tree_node_app_ids_.insert(app_id);
  }
}

void AutomationAXTreeWrapper::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<AXTreeObserver::Change>& changes) {
  DCHECK_EQ(ax_tree(), tree);
  for (const auto& change : changes) {
    AXNode* node = change.node;
    switch (change.type) {
      case NODE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(ax::mojom::Mutation::kNodeCreated, tree,
                                        node);
        break;
      case SUBTREE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(ax::mojom::Mutation::kSubtreeCreated,
                                        tree, node);
        break;
      case NODE_CHANGED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(ax::mojom::Mutation::kNodeChanged, tree,
                                        node);
        break;
      // Unhandled.
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        break;
    }
  }

  for (int id : text_changed_node_ids_) {
    did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
        ax::mojom::Mutation::kTextChanged, tree, tree->GetFromId(id));
  }
  text_changed_node_ids_.clear();
}

void AutomationAXTreeWrapper::OnIgnoredChanged(AXTree* tree,
                                               AXNode* node,
                                               bool is_ignored_new_value) {
  owner_->SendTreeChangeEvent(is_ignored_new_value
                                  ? ax::mojom::Mutation::kNodeRemoved
                                  : ax::mojom::Mutation::kNodeCreated,
                              tree, node);
}

bool AutomationAXTreeWrapper::IsTreeIgnored() {
  // Check the hosting nodes within the parenting trees for ignored host nodes.
  AutomationAXTreeWrapper* tree = this;
  while (tree) {
    AXNode* host = owner_->GetHostInParentTree(&tree);
    if (!host)
      break;

    // This catches things like aria hidden on an iframe.
    if (host->data().HasState(ax::mojom::State::kInvisible))
      return true;
  }
  return false;
}

AXNode* AutomationAXTreeWrapper::GetNodeFromTree(const AXTreeID& tree_id,
                                                 const AXNodeID node_id) const {
  AutomationAXTreeWrapper* tree_wrapper =
      owner_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
  return tree_wrapper ? tree_wrapper->GetNode(node_id) : nullptr;
}

AXTreeID AutomationAXTreeWrapper::GetParentTreeID() const {
  AutomationAXTreeWrapper* parent_tree = GetParentOfTreeId(GetTreeID());
  return parent_tree ? parent_tree->GetTreeID() : AXTreeIDUnknown();
}

AXNode* AutomationAXTreeWrapper::GetParentNodeFromParentTree() const {
  AutomationAXTreeWrapper* wrapper = const_cast<AutomationAXTreeWrapper*>(this);
  return owner_->GetParent(ax_tree_->root(), &wrapper,
                           /* should_use_app_id = */ true,
                           /* requires_unignored = */ false);
}

}  // namespace ui
