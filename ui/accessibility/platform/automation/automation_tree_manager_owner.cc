// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"

#include <map>
#include <set>

#include "base/containers/flat_tree.h"
#include "base/i18n/string_search.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

AutomationTreeManagerOwner::AutomationTreeManagerOwner() : receiver_(this) {}

AutomationTreeManagerOwner::~AutomationTreeManagerOwner() = default;

void AutomationTreeManagerOwner::SendNodesRemovedEvent(
    AXTree* tree,
    const std::vector<int>& ids) {
  AXTreeID tree_id = tree->GetAXTreeID();
  GetAutomationV8Bindings()->SendNodesRemovedEvent(tree_id, ids);
}

bool AutomationTreeManagerOwner::SendTreeChangeEvent(
    ax::mojom::Mutation change_type,
    AXTree* tree,
    AXNode* node) {
  // Notify custom bindings when there's an unloaded tree; js will enable the
  // renderer and wait for it to load.
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    const std::string& child_tree_id_str =
        node->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId);
    AXTreeID child_tree_id = AXTreeID::FromString(child_tree_id_str);
    auto* tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    if (!tree_wrapper || !tree_wrapper->ax_tree()->data().loaded)
      GetAutomationV8Bindings()->SendChildTreeIDEvent(child_tree_id);
  }

  if (!ShouldSendTreeChangeEvent(change_type, tree, node))
    return false;

  AXTreeID tree_id = tree->GetAXTreeID();
  bool did_send_event = false;
  for (const auto& observer : tree_change_observers_) {
    switch (observer.filter) {
      case TreeChangeObserverFilter::kNoTreeChanges:
      default:
        continue;
      case TreeChangeObserverFilter::kLiveRegionTreeChanges:
        if (!node->HasStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus) &&
            node->GetRole() != ax::mojom::Role::kAlert &&
            change_type != ax::mojom::Mutation::kSubtreeUpdateEnd) {
          continue;
        }
        break;
      case TreeChangeObserverFilter::kTextMarkerChanges:
        if (!node->HasIntListAttribute(
                ax::mojom::IntListAttribute::kMarkerTypes))
          continue;
        break;
      case TreeChangeObserverFilter::kAllTreeChanges:
        break;
    }

    did_send_event = true;

    GetAutomationV8Bindings()->SendTreeChangeEvent(observer.id, tree_id,
                                                   node->id(), change_type);
  }

  return did_send_event;
}

void AutomationTreeManagerOwner::SendAutomationEvent(
    AXTreeID tree_id,
    const gfx::Point& mouse_location,
    const AXEvent& event,
    std::optional<AXEventGenerator::Event> generated_event_type) {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return;

  // Event type is none if both the ax event and generated event (if it exists)
  // are type none or ignored.
  bool is_type_none =
      (event.event_type == ax::mojom::Event::kNone ||
       ShouldIgnoreAXEventForAutomation(event.event_type)) &&
      (!generated_event_type.has_value() ||
       (generated_event_type.value() == AXEventGenerator::Event::NONE ||
        ShouldIgnoreGeneratedEventForAutomation(generated_event_type.value())));

  // These events get used internally to trigger other behaviors in js
  // so they should always be fired.
  bool fire_ax_event =
      (event.event_type == ax::mojom::Event::kHitTestResult ||
       event.event_type == ax::mojom::Event::kMediaStartedPlaying ||
       event.event_type == ax::mojom::Event::kMediaStoppedPlaying);
  bool fire_event = is_type_none || fire_ax_event;

  // If we don't explicitly recognize the event type, require a valid, unignored
  // node target.
  AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), event.id);
  if (!fire_event && (!node || node->data().IsIgnored()))
    return;

  std::tuple<ax::mojom::Event, AXEventGenerator::Event> event_type_tuple =
      MakeTupleForAutomationFromEventTypes(event.event_type,
                                           generated_event_type.has_value()
                                               ? generated_event_type.value()
                                               : AXEventGenerator::Event::NONE);

  while (node && tree_wrapper && !fire_event) {
    if (tree_wrapper->HasEventListener(event_type_tuple, node)) {
      fire_event = true;
      break;
    }
    node = GetParent(node, &tree_wrapper);
  }

  if (!fire_event)
    return;

  GetAutomationV8Bindings()->SendAutomationEvent(tree_id, event, mouse_location,
                                                 event_type_tuple);
}

AXNode* AutomationTreeManagerOwner::GetHostInParentTree(
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  AutomationAXTreeWrapper* parent_tree_wrapper = nullptr;

  AXTreeID parent_tree_id =
      (*in_out_tree_wrapper)->ax_tree()->data().parent_tree_id;
  if (parent_tree_id != AXTreeIDUnknown()) {
    // If the tree specifies its parent tree ID, use that. That provides
    // some additional security guarantees, so a tree can't be "claimed"
    // by something else.
    parent_tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(parent_tree_id);
  } else {
    // Otherwise if it was unspecified, check to see if another tree listed
    // this one as its child, and then we know the parent.
    parent_tree_wrapper = AutomationAXTreeWrapper::GetParentOfTreeId(
        (*in_out_tree_wrapper)->GetTreeID());
  }

  if (!parent_tree_wrapper)
    return nullptr;

  std::set<int32_t> host_node_ids =
      parent_tree_wrapper->ax_tree()->GetNodeIdsForChildTreeId(
          (*in_out_tree_wrapper)->GetTreeID());

#if !defined(NDEBUG)
  if (host_node_ids.size() > 1)
    DLOG(WARNING) << "Multiple nodes claim the same child tree id.";
#endif

  for (int32_t host_node_id : host_node_ids) {
    AXNode* host_node = parent_tree_wrapper->GetNodeFromTree(
        parent_tree_wrapper->GetTreeID(), host_node_id);
    if (host_node) {
      DCHECK_EQ((*in_out_tree_wrapper)->GetTreeID(),
                AXTreeID::FromString(host_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kChildTreeId)));
      *in_out_tree_wrapper = parent_tree_wrapper;
      return host_node;
    }
  }

  return nullptr;
}

AutomationAXTreeWrapper*
AutomationTreeManagerOwner::GetAutomationAXTreeWrapperFromTreeID(
    AXTreeID tree_id) const {
  const auto iter = tree_id_to_tree_wrapper_map_.find(tree_id);
  if (iter == tree_id_to_tree_wrapper_map_.end())
    return nullptr;

  return iter->second.get();
}

AXNode* AutomationTreeManagerOwner::GetParent(
    AXNode* node,
    AutomationAXTreeWrapper** in_out_tree_wrapper,
    bool should_use_app_id,
    bool requires_unignored) const {
  if (should_use_app_id &&
      node->HasStringAttribute(ax::mojom::StringAttribute::kAppId)) {
    AXNode* parent_app_node =
        AutomationAXTreeWrapper::GetParentTreeNodeForAppID(
            node->GetStringAttribute(ax::mojom::StringAttribute::kAppId), this);
    if (parent_app_node) {
      *in_out_tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(
          parent_app_node->tree()->GetAXTreeID());
      return parent_app_node;
    }
  }

  AXNode* parent = nullptr;
  if (!requires_unignored) {
    parent = node->GetParent();
    if (parent)
      return parent;
    return GetHostInParentTree(in_out_tree_wrapper);
  }

  parent = node->GetUnignoredParent();
  if (!parent) {
    // Search up ancestor trees until we find one with a host that is unignored.
    while ((parent = GetHostInParentTree(in_out_tree_wrapper))) {
      if (*in_out_tree_wrapper && !(*in_out_tree_wrapper)->IsTreeIgnored())
        break;
    }

    if (parent && parent->IsIgnored())
      parent = parent->GetUnignoredParent();
  }

  return parent;
}

void AutomationTreeManagerOwner::MaybeSendFocusAndBlur(
    AutomationAXTreeWrapper* tree,
    const AXTreeID& tree_id,
    const std::vector<AXTreeUpdate>& updates,
    const std::vector<AXEvent>& events,
    gfx::Point mouse_location) {
  AXNode* old_node = nullptr;
  AutomationAXTreeWrapper* old_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(focus_tree_id_);
  if (old_wrapper) {
    old_node =
        old_wrapper->GetNodeFromTree(old_wrapper->GetTreeID(), focus_id_);
  }

  // Determine whether old focus was lost.
  bool lost_old_focus = old_node == nullptr;

  // Determine whether there's a focus or blur event and take its event from.
  // Also, save the raw event target (tree + node).
  ax::mojom::EventFrom event_from = ax::mojom::EventFrom::kNone;
  ax::mojom::Action event_from_action = ax::mojom::Action::kNone;
  AXNodeData::AXID raw_focus_target_id = AXNodeData::kInvalidAXID;
  bool event_bundle_has_focus_or_blur = false;
  for (const auto& event : events) {
    bool is_blur = event.event_type == ax::mojom::Event::kBlur;
    bool is_focus = event.event_type == ax::mojom::Event::kFocus;
    if (is_blur || is_focus) {
      event_from = event.event_from;
      event_from_action = event.event_from_action;
      event_bundle_has_focus_or_blur = true;
    }

    if (is_focus)
      raw_focus_target_id = event.id;
  }

  AutomationAXTreeWrapper* desktop_tree =
      GetAutomationAXTreeWrapperFromTreeID(desktop_tree_id_);
  AXNode* new_node = nullptr;
  AutomationAXTreeWrapper* new_wrapper = nullptr;
  if (desktop_tree && !GetFocusInternal(desktop_tree, &new_wrapper, &new_node))
    return;

  if (!desktop_tree) {
    // Can occur if the extension does not have desktop permission,
    // chrome.automation.getDesktop has yet to be called, or if this platform
    // does not support Aura.
    new_wrapper = tree;
    new_node = tree->ax_tree()->GetFromId(raw_focus_target_id);
    if (!new_node)
      return;
  }

  bool same_focused_tree = old_wrapper == new_wrapper;

  // Return if focus didn't change.
  if (same_focused_tree && new_node == old_node)
    return;

  bool is_from_desktop = tree->IsDesktopTree();

  // Require an explicit focus event on non-desktop trees, when focus moves
  // within them, with an old focused node.
  if (!event_bundle_has_focus_or_blur && !lost_old_focus && !is_from_desktop &&
      same_focused_tree)
    return;

  // Blur previous focus.
  if (old_node) {
    AXEvent blur_event;
    blur_event.id = old_node->id();
    blur_event.event_from = event_from;
    blur_event.event_from_action = event_from_action;
    blur_event.event_type = ax::mojom::Event::kBlur;
    SendAutomationEvent(old_wrapper->GetTreeID(), mouse_location, blur_event);

    focus_id_ = -1;
    focus_tree_id_ = AXTreeIDUnknown();
  }

  // New focus.
  if (new_node) {
    AXEvent focus_event;
    focus_event.id = new_node->id();
    focus_event.event_from = event_from;
    focus_event.event_from_action = event_from_action;
    focus_event.event_type = ax::mojom::Event::kFocus;
    SendAutomationEvent(new_wrapper->GetTreeID(), mouse_location, focus_event);
    focus_id_ = new_node->id();
    focus_tree_id_ = new_wrapper->GetTreeID();
  }
}

std::optional<gfx::Rect>
AutomationTreeManagerOwner::GetAccessibilityFocusedLocation() const {
  if (accessibility_focused_tree_id_ == AXTreeIDUnknown())
    return std::nullopt;

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(accessibility_focused_tree_id_);
  if (!tree_wrapper)
    return std::nullopt;

  AXNode* node = tree_wrapper->GetAccessibilityFocusedNode();
  if (!node)
    return std::nullopt;

  return ComputeGlobalNodeBounds(tree_wrapper, node);
}

void AutomationTreeManagerOwner::SendAccessibilityFocusedLocationChange(
    const gfx::Point& mouse_location) {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(accessibility_focused_tree_id_);
  if (!tree_wrapper)
    return;

  AXEvent event;
  event.id = tree_wrapper->accessibility_focused_id();
  event.event_type = ax::mojom::Event::kLocationChanged;
  SendAutomationEvent(accessibility_focused_tree_id_, mouse_location, event);
}

bool AutomationTreeManagerOwner::GetFocusInternal(
    AutomationAXTreeWrapper* tree_wrapper,
    AutomationAXTreeWrapper** out_tree_wrapper,
    AXNode** out_node) const {
  int focus_id = tree_wrapper->ax_tree()->data().focus_id;
  AXNode* focus =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), focus_id);
  if (!focus)
    return false;

  while (true) {
    // If the focused node is the owner of a child tree, that indicates
    // a node within the child tree is the one that actually has focus.
    const std::string& child_tree_id_str =
        focus->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId);
    const std::string& child_tree_node_app_id_str = focus->GetStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId);

    if (child_tree_id_str.empty() && child_tree_node_app_id_str.empty())
      break;

    AutomationAXTreeWrapper* child_tree_wrapper = nullptr;

    if (!child_tree_node_app_id_str.empty()) {
      std::vector<AXNode*> child_app_nodes =
          AutomationAXTreeWrapper::GetChildTreeNodesForAppID(
              child_tree_node_app_id_str, this);
      if (!child_app_nodes.empty()) {
        // It doesn't matter which app node we use to move to the parent tree.
        AXNode* child_app_node = child_app_nodes[0];
        auto* wrapper = GetAutomationAXTreeWrapperFromTreeID(
            child_app_node->tree()->GetAXTreeID());
        child_tree_wrapper = wrapper;
      }
    }

    // Try to keep following focus recursively, by letting |tree_id| be the
    // new subtree to search in, while keeping |focus_tree_id| set to the tree
    // where we know we found a focused node.
    if (!child_tree_wrapper && !child_tree_id_str.empty()) {
      AXTreeID child_tree_id = AXTreeID::FromString(child_tree_id_str);
      child_tree_wrapper = GetAutomationAXTreeWrapperFromTreeID(child_tree_id);
    }

    if (!child_tree_wrapper)
      break;

    // If |child_tree_wrapper| is a frame tree that indicates a focused frame,
    // jump to that frame if possible.
    AXTreeID focused_tree_id =
        child_tree_wrapper->ax_tree()->data().focused_tree_id;
    if (focused_tree_id != AXTreeIDUnknown() &&
        !child_tree_wrapper->IsDesktopTree()) {
      AutomationAXTreeWrapper* focused_tree_wrapper =
          GetAutomationAXTreeWrapperFromTreeID(
              child_tree_wrapper->ax_tree()->data().focused_tree_id);
      if (focused_tree_wrapper)
        child_tree_wrapper = focused_tree_wrapper;
    }

    int child_focus_id = child_tree_wrapper->ax_tree()->data().focus_id;
    AXNode* child_focus = child_tree_wrapper->GetNodeFromTree(
        child_tree_wrapper->GetTreeID(), child_focus_id);
    if (!child_focus)
      break;

    focus = child_focus;
    tree_wrapper = child_tree_wrapper;
  }

  *out_tree_wrapper = tree_wrapper;
  *out_node = focus;
  return true;
}

gfx::Rect AutomationTreeManagerOwner::ComputeGlobalNodeBounds(
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    gfx::RectF local_bounds,
    bool* offscreen,
    bool clip_bounds) const {
  gfx::RectF bounds = local_bounds;

  bool crossed_app_id = false;
  while (node) {
    bounds = tree_wrapper->ax_tree()->RelativeToTreeBounds(
        node, bounds, offscreen, clip_bounds,
        /* skip_container_offset = */ crossed_app_id);

    bool should_use_app_id = tree_wrapper->ax_tree()->root() == node;
    AutomationAXTreeWrapper* previous_tree_wrapper = tree_wrapper;
    AXNode* parent_of_root = GetParent(tree_wrapper->ax_tree()->root(),
                                       &tree_wrapper, should_use_app_id);
    if (parent_of_root == node)
      break;

    // This is a fallback for trees that are constructed using app ids. Do the
    // least possible expensive check here.
    crossed_app_id = false;
    if (!parent_of_root && previous_tree_wrapper->GetParentTreeFromAnyAppID()) {
      // Since the tree has a valid child tree app id pointing to a valid tree,
      // walk the ancestry of |node| to find the specific app id and resolve to
      // the parent tree node.
      AXNode* found_node = node;
      while (found_node && !found_node->HasStringAttribute(
                               ax::mojom::StringAttribute::kAppId)) {
        found_node = found_node->parent();
      }
      if (found_node) {
        const std::string& app_id =
            found_node->GetStringAttribute(ax::mojom::StringAttribute::kAppId);
        parent_of_root =
            AutomationAXTreeWrapper::GetParentTreeNodeForAppID(app_id, this);
        tree_wrapper =
            AutomationAXTreeWrapper::GetParentTreeWrapperForAppID(app_id, this);
        crossed_app_id = true;
      }
    }

    // When crossing out of a tree that has a device scale factor into a tree
    // that does not, unscale by the device scale factor.
    if (previous_tree_wrapper->HasDeviceScaleFactor() &&
        !tree_wrapper->HasDeviceScaleFactor()) {
      // TODO(crbug/1234225): This calculation should be included in
      // |AXRelativeBounds::transform|.
      const float scale_factor = parent_of_root->data().GetFloatAttribute(
          ax::mojom::FloatAttribute::kChildTreeScale);
      if (scale_factor > 0)
        bounds.Scale(1.0 / scale_factor);
    }

    node = parent_of_root;
  }

  return gfx::ToEnclosingRect(bounds);
}

std::vector<AXNode*> AutomationTreeManagerOwner::GetRootsOfChildTree(
    AXNode* node) const {
  if (!node->HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId) &&
      !node->HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
    return std::vector<AXNode*>();
  }

  // Account for two types of links to child trees.
  // An explicit tree id to a child tree.
  const std::string& child_tree_id_str =
      node->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId);

  // A node attribute pointing to a node in a descendant tree.
  const std::string& child_tree_node_app_id_str =
      node->GetStringAttribute(ax::mojom::StringAttribute::kChildTreeNodeAppId);

  if (!child_tree_node_app_id_str.empty()) {
    std::vector<AXNode*> child_app_nodes =
        AutomationAXTreeWrapper::GetChildTreeNodesForAppID(
            child_tree_node_app_id_str, this);
    if (!child_app_nodes.empty())
      return child_app_nodes;
  }

  AutomationAXTreeWrapper* child_tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(
          AXTreeID::FromString(child_tree_id_str));
  if (!child_tree_wrapper || !child_tree_wrapper->ax_tree()->root())
    return std::vector<AXNode*>();

  return {child_tree_wrapper->ax_tree()->root()};
}

AXNode* AutomationTreeManagerOwner::GetNextInTreeOrder(
    AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  auto iter = start->UnignoredChildrenBegin();
  if (iter != start->UnignoredChildrenEnd())
    return &(*iter);

  AXNode* walker = start;

  // We also have to check child tree id.
  std::vector<AXNode*> child_roots = GetRootsOfChildTree(walker);
  if (!child_roots.empty())
    return child_roots[0];

  // Find the next branch forward.
  AXNode* parent;
  while ((parent = GetParent(walker, in_out_tree_wrapper))) {
    // TODO(accessibility): convert below to use UnignoredChildIterator.
    if ((walker->GetUnignoredIndexInParent() + 1) <
        parent->GetUnignoredChildCount()) {
      return parent->GetUnignoredChildAtIndex(
          walker->GetUnignoredIndexInParent() + 1);
    }
    walker = parent;
  }

  return nullptr;
}

AXNode* AutomationTreeManagerOwner::GetPreviousInTreeOrder(
    AXNode* start,
    AutomationAXTreeWrapper** in_out_tree_wrapper) const {
  AXNode* walker = start;

  AXNode* parent = GetParent(start, in_out_tree_wrapper);

  // No more nodes.
  if (!parent)
    return nullptr;

  // No previous sibling; parent is previous.
  if (walker->GetUnignoredIndexInParent() == 0)
    return parent;

  walker =
      parent->GetUnignoredChildAtIndex(walker->GetUnignoredIndexInParent() - 1);

  // Walks to deepest last child.
  while (true) {
    auto iter = walker->UnignoredChildrenBegin();
    if (iter != walker->UnignoredChildrenEnd() &&
        --iter != walker->UnignoredChildrenEnd()) {
      walker = &(*iter);
    } else if (GetRootsOfChildTree(walker).empty()) {
      break;
    }
  }
  return walker;
}

std::vector<int> AutomationTreeManagerOwner::CalculateSentenceBoundary(
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    bool start_boundary) {
  // Create an empty vector for storing final results and deal with the node
  // without a name.
  std::vector<int> sentence_boundary;
  std::u16string node_name =
      node->GetString16Attribute(ax::mojom::StringAttribute::kName);
  if (node_name.empty()) {
    return sentence_boundary;
  }

  // We will calculate the boundary of a combined string, which consists
  // of|pre_str|, |post_str|. When the node is inside a paragraph, the |pre_str|
  // is the string from the beginning of the paragraph to the head of current
  // node. The |post_str| is the string from the head of current node to the end
  // of the paragraph.
  std::u16string pre_str;
  std::u16string post_str;
  AXNodePosition::AXPositionInstance head_pos =
      AXNodePosition::CreatePosition(*node, 0 /* child_index_or_text_offset */,
                                     ax::mojom::TextAffinity::kDownstream)
          ->CreatePositionAtStartOfAnchor();

  // If the head of current node is not at the start of paragraph, we need to
  // change the empty |pre_str| with the string from the beginning of the
  // paragraph to the head of current node.
  if (!head_pos->AtStartOfParagraph()) {
    AXNodePosition::AXPositionInstance start_para_pos =
        head_pos->CreatePreviousParagraphStartPosition(
            {AXBoundaryBehavior::kStopAtLastAnchorBoundary,
             AXBoundaryDetection::kDontCheckInitialPosition});
    AXRange<AXPosition<AXNodePosition, AXNode>> pre_range(
        start_para_pos->Clone(), head_pos->Clone());
    pre_str = pre_range.GetText();
  }

  // Change the empty |post_str| with the string from the head of the current
  // node to the end of the paragraph.
  AXNodePosition::AXPositionInstance end_para_pos =
      head_pos->CreateNextParagraphEndPosition(
          {AXBoundaryBehavior::kStopAtLastAnchorBoundary,
           AXBoundaryDetection::kDontCheckInitialPosition});
  AXRange<AXPosition<AXNodePosition, AXNode>> post_range(head_pos->Clone(),
                                                         end_para_pos->Clone());
  post_str = post_range.GetText();

  // Calculate the boundary of the |combined_str|.
  std::u16string combined_str = pre_str + post_str;
  auto boundary_func =
      start_boundary ? &GetSentenceStartOffsets : &GetSentenceEndOffsets;
  std::vector<int> combined_sentence_boundary = boundary_func(combined_str);

  // To get the final result, we need to get rid of indexes that do not belong
  // to the current node. First, we subtract the length of |pre_str| from the
  // |combined_sentence_boundary| vector. Then, we only save the non-negative
  // elements that equal or smaller than |max_index| into |sentence_boundary|.
  // Note that an end boundary index can be outside of the current node, thus
  // the |max_index| is set to the length of |node_name|.
  int pre_str_length = pre_str.length();
  int max_index = start_boundary ? node_name.length() - 1 : node_name.length();
  for (int& index : combined_sentence_boundary) {
    index -= pre_str_length;
    if (index >= 0 && index <= max_index) {
      sentence_boundary.push_back(index);
    }
  }
  return sentence_boundary;
}

bool AutomationTreeManagerOwner::GetFocus(AXTreeID* focused_tree_id,
                                          int* node_id) const {
  AutomationAXTreeWrapper* desktop_tree =
      GetAutomationAXTreeWrapperFromTreeID(desktop_tree_id_);
  AutomationAXTreeWrapper* focused_wrapper = nullptr;
  AXNode* focused_node = nullptr;
  if (desktop_tree &&
      !GetFocusInternal(desktop_tree, &focused_wrapper, &focused_node))
    return false;

  if (!desktop_tree) {
    focused_wrapper = GetAutomationAXTreeWrapperFromTreeID(focus_tree_id_);
    if (!focused_wrapper)
      return false;

    focused_node = focused_wrapper->GetNodeFromTree(
        focused_wrapper->GetTreeID(), focus_id_);
    if (!focused_node)
      return false;
  }

  *focused_tree_id = focused_wrapper->GetTreeID();
  *node_id = focused_node->id();
  return true;
}

size_t AutomationTreeManagerOwner::GetChildCount(AXNode* node) const {
  std::vector<AXNode*> child_roots = GetRootsOfChildTree(node);
  if (child_roots.empty())
    return node->GetUnignoredChildCount();
  else
    return child_roots.size();
}

std::vector<int> AutomationTreeManagerOwner::GetChildIDs(
    AXNode* node,
    AXTreeID* result_tree_id) const {
  std::vector<int> child_ids;
  std::vector child_roots = GetRootsOfChildTree(node);
  if (!child_roots.empty()) {
    *result_tree_id = child_roots[0]->tree()->GetAXTreeID();
    for (AXNode* child_root : child_roots)
      child_ids.push_back(child_root->id());
  } else {
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      child_ids.push_back(iter->id());
      *result_tree_id = iter->tree()->GetAXTreeID();
    }
  }
  return child_ids;
}

bool AutomationTreeManagerOwner::GetBoundsForRange(
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    int start,
    int end,
    bool clipped,
    gfx::Rect* result) const {
  if (node->GetRole() != ax::mojom::Role::kInlineTextBox)
    return false;

  // Use character offsets to compute the local bounds of this subrange.
  gfx::RectF local_bounds(0, 0, node->data().relative_bounds.bounds.width(),
                          node->data().relative_bounds.bounds.height());
  const std::string& name =
      node->GetStringAttribute(ax::mojom::StringAttribute::kName);
  std::vector<int> character_offsets =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  int len = static_cast<int>(std::min(name.size(), character_offsets.size()));
  if (start >= 0 && start <= end && end <= len) {
    int start_offset = start > 0 ? character_offsets[start - 1] : 0;
    int end_offset = end > 0 ? character_offsets[end - 1] : 0;

    switch (node->data().GetTextDirection()) {
      case ax::mojom::WritingDirection::kLtr:
      default:
        local_bounds.set_x(local_bounds.x() + start_offset);
        local_bounds.set_width(end_offset - start_offset);
        break;
      case ax::mojom::WritingDirection::kRtl:
        local_bounds.set_x(local_bounds.x() + local_bounds.width() -
                           end_offset);
        local_bounds.set_width(end_offset - start_offset);
        break;
      case ax::mojom::WritingDirection::kTtb:
        local_bounds.set_y(local_bounds.y() + start_offset);
        local_bounds.set_height(end_offset - start_offset);
        break;
      case ax::mojom::WritingDirection::kBtt:
        local_bounds.set_y(local_bounds.y() + local_bounds.height() -
                           end_offset);
        local_bounds.set_height(end_offset - start_offset);
        break;
    }
  }

  // Convert from local to global coordinates second, after subsetting,
  // because the local to global conversion might involve matrix
  // transformations.
  *result = ComputeGlobalNodeBounds(tree_wrapper, node, local_bounds, nullptr,
                                    clipped /* clip_bounds */);
  return true;
}

const char* AutomationTreeManagerOwner::GetName(AXNode* node) const {
  if (node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    return node->GetStringAttribute(ax::mojom::StringAttribute::kName).c_str();
  }

  return nullptr;
}

bool AutomationTreeManagerOwner::GetNextTextMatch(
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    const std::string& search_str,
    bool backward,
    AXTreeID* result_tree_id,
    int* result_node_id) const {
  std::u16string search_str_16 = base::UTF8ToUTF16(search_str);
  auto next = backward ? &AutomationTreeManagerOwner::GetPreviousInTreeOrder
                       : &AutomationTreeManagerOwner::GetNextInTreeOrder;
  AutomationAXTreeWrapper** target_tree_wrapper = &tree_wrapper;
  while (true) {
    node = (this->*next)(node, target_tree_wrapper);

    // We explicitly disallow searches in the desktop tree.
    if ((*target_tree_wrapper)->IsDesktopTree())
      return false;

    if (!node)
      return false;

    if (!node->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      continue;
    }

    std::u16string name =
        node->GetString16Attribute(ax::mojom::StringAttribute::kName);
    if (base::i18n::StringSearchIgnoringCaseAndAccents(search_str_16, name,
                                                       nullptr, nullptr)) {
      *result_tree_id = (*target_tree_wrapper)->GetTreeID();
      *result_node_id = node->id();
      return true;
    }
  }
}

bool AutomationTreeManagerOwner::GetChildIDAtIndex(const AXTreeID& tree_id,
                                                   int node_id,
                                                   int index,
                                                   AXTreeID* child_tree_id,
                                                   int* child_node_id) const {
  if (index < 0)
    return false;

  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return false;

  AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node)
    return false;

  // Check for child roots.
  std::vector<AXNode*> child_roots = GetRootsOfChildTree(node);

  AXNode* child_node = nullptr;
  if (!child_roots.empty() && static_cast<size_t>(index) < child_roots.size()) {
    child_node = child_roots[index];
  } else if (node->IsIgnored()) {
    return false;
  } else if (static_cast<size_t>(index) >= node->GetUnignoredChildCount()) {
    return false;
  } else {
    child_node = node->GetUnignoredChildAtIndex(static_cast<size_t>(index));
  }

  DCHECK(child_node);
  *child_tree_id = child_node->tree()->GetAXTreeID();
  *child_node_id = child_node->id();
  return true;
}

bool AutomationTreeManagerOwner::GetAccessibilityFocus(AXTreeID* tree_id,
                                                       int* node_id) const {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(accessibility_focused_tree_id_);
  if (!tree_wrapper)
    return false;

  AXNode* node = tree_wrapper->GetAccessibilityFocusedNode();
  if (!node)
    return false;

  *tree_id = accessibility_focused_tree_id_;
  *node_id = node->id();
  return true;
}

AXNode* AutomationTreeManagerOwner::GetNodeFromTree(const AXTreeID& tree_id,
                                                    int node_id) const {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return nullptr;

  return tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
}

void AutomationTreeManagerOwner::AddTreeChangeObserver(
    int observer_id,
    TreeChangeObserverFilter filter) {
  TreeChangeObserver observer;
  observer.id = observer_id;
  observer.filter = filter;
  tree_change_observers_.push_back(observer);
  UpdateOverallTreeChangeObserverFilter();
}

void AutomationTreeManagerOwner::RemoveTreeChangeObserver(int observer_id) {
  for (auto iter = tree_change_observers_.begin();
       iter != tree_change_observers_.end(); ++iter) {
    if (iter->id == observer_id) {
      tree_change_observers_.erase(iter);
      break;
    }
  }

  UpdateOverallTreeChangeObserverFilter();
}

bool AutomationTreeManagerOwner::ShouldSendTreeChangeEvent(
    ax::mojom::Mutation change_type,
    AXTree* tree,
    AXNode* node) const {
  // Don't bother dispatching to js if the node is ignored. A js
  // client shouldn't process ignored nodes.
  if (node->IsIgnored())
    return false;

  // Likewise, don't process tree changes on ignored trees.
  auto* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree->GetAXTreeID());
  if (!tree_wrapper || tree_wrapper->IsTreeIgnored())
    return false;

  bool has_filter = false;
  if (tree_change_observer_overall_filter_ &
      (1 << TreeChangeObserverFilter::kLiveRegionTreeChanges)) {
    if (node->HasStringAttribute(
            ax::mojom::StringAttribute::kContainerLiveStatus) ||
        node->GetRole() == ax::mojom::Role::kAlert ||
        change_type == ax::mojom::Mutation::kSubtreeUpdateEnd) {
      has_filter = true;
    }
  }

  if (tree_change_observer_overall_filter_ &
      (1 << TreeChangeObserverFilter::kTextMarkerChanges)) {
    if (node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes))
      has_filter = true;
  }

  if (tree_change_observer_overall_filter_ &
      (1 << TreeChangeObserverFilter::kAllTreeChanges))
    has_filter = true;

  return has_filter;
}

void AutomationTreeManagerOwner::DestroyAccessibilityTree(
    const AXTreeID& tree_id) {
  auto& child_tree_id_reverse_map =
      AutomationAXTreeWrapper::GetChildTreeIDReverseMap();
  std::erase_if(
      child_tree_id_reverse_map,
      [tree_id](const std::pair<AXTreeID, AutomationAXTreeWrapper*>& pair) {
        return pair.first == tree_id || pair.second->GetTreeID() == tree_id;
      });

  if (tree_id == accessibility_focused_tree_id_)
    accessibility_focused_tree_id_ = AXTreeIDUnknown();

  RemoveAutomationTreeWrapperFromCache(tree_id);
  trees_with_event_listeners_.erase(tree_id);
}

bool AutomationTreeManagerOwner::CalculateNodeState(const AXTreeID& tree_id,
                                                    int node_id,
                                                    uint32_t* node_state,
                                                    bool* offscreen,
                                                    bool* focused) const {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper)
    return false;

  AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node)
    return false;

  *node_state = node->data().state;

  AutomationAXTreeWrapper* top_tree_wrapper = nullptr;
  AutomationAXTreeWrapper* walker = tree_wrapper;
  while (walker && walker != top_tree_wrapper) {
    top_tree_wrapper = walker;
    GetParent(walker->ax_tree()->root(), &walker);
  }

  *focused = tree_wrapper->IsInFocusChain(node->id());
  ComputeGlobalNodeBounds(tree_wrapper, node, gfx::RectF(), offscreen);
  return true;
}

void AutomationTreeManagerOwner::SetAccessibilityFocus(AXTreeID tree_id) {
  if (tree_id != accessibility_focused_tree_id_ &&
      accessibility_focused_tree_id_ != AXTreeIDUnknown()) {
    AutomationAXTreeWrapper* previous_tree_wrapper =
        GetAutomationAXTreeWrapperFromTreeID(accessibility_focused_tree_id_);
    if (previous_tree_wrapper) {
      previous_tree_wrapper->SetAccessibilityFocus(kInvalidAXNodeID);
    }
  }
  accessibility_focused_tree_id_ = tree_id;
}

void AutomationTreeManagerOwner::CacheAutomationTreeWrapperForTreeID(
    const AXTreeID& tree_id,
    AutomationAXTreeWrapper* tree_wrapper) {
  tree_id_to_tree_wrapper_map_.insert(
      std::make_pair(tree_id, base::WrapUnique(tree_wrapper)));
}

void AutomationTreeManagerOwner::RemoveAutomationTreeWrapperFromCache(
    const AXTreeID& tree_id) {
  tree_id_to_tree_wrapper_map_.erase(tree_id);
}

void AutomationTreeManagerOwner::ClearCachedAutomationTreeWrappers() {
  tree_id_to_tree_wrapper_map_.clear();
}

void AutomationTreeManagerOwner::ClearCachedAccessibilityTrees() {
  tree_change_observers_.clear();
  ClearCachedAutomationTreeWrappers();
  AutomationAXTreeWrapper::GetChildTreeIDReverseMap().clear();
}

void AutomationTreeManagerOwner::Invalidate() {
  auto& child_tree_id_reverse_map =
      AutomationAXTreeWrapper::GetChildTreeIDReverseMap();
  std::erase_if(
      child_tree_id_reverse_map,
      [this](const std::pair<AXTreeID, AutomationAXTreeWrapper*>& pair) {
        return pair.second->owner() == this;
      });

  ClearCachedAutomationTreeWrappers();
}

void AutomationTreeManagerOwner::TreeEventListenersChanged(
    AutomationAXTreeWrapper* tree_wrapper) {
  if (tree_wrapper->EventListenerCount() != 0) {
    trees_with_event_listeners_.insert(tree_wrapper->GetTreeID());
    return;
  }

  if (trees_with_event_listeners_.empty())
    return;

  trees_with_event_listeners_.erase(tree_wrapper->GetTreeID());
  if (!trees_with_event_listeners_.empty())
    return;

  NotifyTreeEventListenersChanged();
}

bool AutomationTreeManagerOwner::HasTreesWithEventListeners() const {
  return !trees_with_event_listeners_.empty();
}

void AutomationTreeManagerOwner::
    MaybeSendOnAllAutomationEventListenersRemoved() {
  if (HasTreesWithEventListeners())
    return;

  GetAutomationV8Bindings()->SendOnAllEventListenersRemoved();
}

void AutomationTreeManagerOwner::UpdateOverallTreeChangeObserverFilter() {
  tree_change_observer_overall_filter_ = 0;
  for (const auto& observer : tree_change_observers_)
    tree_change_observer_overall_filter_ |= 1 << observer.filter;
}

void AutomationTreeManagerOwner::DispatchTreeDestroyedEvent(
    const AXTreeID& tree_id) {
  GetAutomationV8Bindings()->SendTreeDestroyedEvent(tree_id);
}

void AutomationTreeManagerOwner::DispatchAccessibilityEvents(
    const AXTreeID& tree_id,
    const std::vector<AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<AXEvent>& events) {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  bool is_new_tree = tree_wrapper == nullptr;
  if (is_new_tree) {
    tree_wrapper = new AutomationAXTreeWrapper(this);
    CacheAutomationTreeWrapperForTreeID(tree_id, tree_wrapper);
  }

  if (!tree_wrapper->OnAccessibilityEvents(tree_id, updates, events,
                                           mouse_location)) {
    DLOG(ERROR) << tree_wrapper->ax_tree()->error();
    GetAutomationV8Bindings()->SendTreeSerializationError(tree_id);
    return;
  }

  // Send an initial event to ensure the js-side objects get created for new
  // trees.
  if (is_new_tree) {
    AXEvent initial_event;
    initial_event.id = -1;
    initial_event.event_from = ax::mojom::EventFrom::kNone;
    initial_event.event_type = ax::mojom::Event::kNone;
    SendAutomationEvent(tree_id, gfx::Point(), initial_event);
  }

  // After handling events in js, if the client did not add any event listeners,
  // shut things down.
  TreeEventListenersChanged(tree_wrapper);
}

void AutomationTreeManagerOwner::DispatchAccessibilityLocationChange(
    const AXTreeID& tree_id,
    int32_t node_id,
    const AXRelativeBounds& bounds) {
  AutomationAXTreeWrapper* tree_wrapper =
      GetAutomationAXTreeWrapperFromTreeID(tree_id);
  if (!tree_wrapper) {
    return;
  }
  AXNode* node =
      tree_wrapper->GetNodeFromTree(tree_wrapper->GetTreeID(), node_id);
  if (!node) {
    return;
  }

  std::optional<gfx::Rect> previous_accessibility_focused_global_bounds =
      GetAccessibilityFocusedLocation();

  node->SetLocation(bounds.offset_container_id, bounds.bounds,
                    bounds.transform.get());

  if (previous_accessibility_focused_global_bounds.has_value() &&
      previous_accessibility_focused_global_bounds !=
          GetAccessibilityFocusedLocation()) {
    SendAccessibilityFocusedLocationChange(gfx::Point());
  }
}

void AutomationTreeManagerOwner::DispatchActionResult(const AXActionData& data,
                                                      bool result) {
  GetAutomationV8Bindings()->SendActionResultEvent(data, result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AutomationTreeManagerOwner::DispatchGetTextLocationResult(
    const AXActionData& data,
    const std::optional<gfx::Rect>& rect) {
  GetAutomationV8Bindings()->SendGetTextLocationResult(data, rect);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace ui
