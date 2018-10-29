// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "base/stl_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

AXEventGenerator::EventParams::EventParams(Event event,
                                           ax::mojom::EventFrom event_from)
    : event(event), event_from(event_from) {}

AXEventGenerator::TargetedEvent::TargetedEvent(ui::AXNode* node,
                                               const EventParams& event_params)
    : node(node), event_params(event_params) {}

bool AXEventGenerator::EventParams::operator==(const EventParams& rhs) {
  return rhs.event == event;
}

bool AXEventGenerator::EventParams::operator<(const EventParams& rhs) const {
  return event < rhs.event;
}

AXEventGenerator::Iterator::Iterator(
    const std::map<AXNode*, std::set<EventParams>>& map,
    const std::map<AXNode*, std::set<EventParams>>::const_iterator& head)
    : map_(map), map_iter_(head) {
  if (map_iter_ != map.end())
    set_iter_ = map_iter_->second.begin();
}

AXEventGenerator::Iterator::Iterator(const AXEventGenerator::Iterator& other) =
    default;

AXEventGenerator::Iterator::~Iterator() = default;

bool AXEventGenerator::Iterator::operator!=(
    const AXEventGenerator::Iterator& rhs) const {
  return map_iter_ != rhs.map_iter_ ||
         (map_iter_ != map_.end() && set_iter_ != rhs.set_iter_);
}

AXEventGenerator::Iterator& AXEventGenerator::Iterator::operator++() {
  if (map_iter_ == map_.end())
    return *this;

  set_iter_++;
  while (map_iter_ != map_.end() && set_iter_ == map_iter_->second.end()) {
    map_iter_++;
    if (map_iter_ != map_.end())
      set_iter_ = map_iter_->second.begin();
  }

  return *this;
}

AXEventGenerator::TargetedEvent AXEventGenerator::Iterator::operator*() const {
  DCHECK(map_iter_ != map_.end() && set_iter_ != map_iter_->second.end());
  return AXEventGenerator::TargetedEvent(map_iter_->first, *set_iter_);
}

AXEventGenerator::AXEventGenerator() = default;

AXEventGenerator::AXEventGenerator(AXTree* tree) : tree_(tree) {
  if (tree_)
    tree_->SetDelegate(this);
}

AXEventGenerator::~AXEventGenerator() {
  if (tree_)
    tree_->SetDelegate(nullptr);
}

void AXEventGenerator::SetTree(AXTree* new_tree) {
  if (tree_)
    tree_->SetDelegate(nullptr);
  tree_ = new_tree;
  if (tree_)
    tree_->SetDelegate(this);
}

void AXEventGenerator::ReleaseTree() {
  tree_ = nullptr;
}

void AXEventGenerator::ClearEvents() {
  tree_events_.clear();
}

void AXEventGenerator::AddEvent(ui::AXNode* node,
                                AXEventGenerator::Event event) {
  if (node->data().role == ax::mojom::Role::kInlineTextBox)
    return;

  // A newly created live region or alert should not *also* fire a
  // live region changed event.
  if (event == Event::LIVE_REGION_CHANGED &&
      (base::ContainsKey(
           tree_events_[node],
           EventParams(Event::ALERT, ax::mojom::EventFrom::kNone)) ||
       base::ContainsKey(tree_events_[node],
                         EventParams(Event::LIVE_REGION_CREATED,
                                     ax::mojom::EventFrom::kNone)))) {
    return;
  }

  tree_events_[node].insert(EventParams(event, event_from_));
}

void AXEventGenerator::OnNodeDataWillChange(AXTree* tree,
                                            const AXNodeData& old_node_data,
                                            const AXNodeData& new_node_data) {
  DCHECK_EQ(tree_, tree);
  // Fire CHILDREN_CHANGED events when the list of children updates.
  // Internally we store inline text box nodes as children of a static text
  // node, which enables us to determine character bounds and line layout.
  // We don't expose those to platform APIs, though, so suppress
  // CHILDREN_CHANGED events on static text nodes.
  if (new_node_data.child_ids != old_node_data.child_ids &&
      new_node_data.role != ax::mojom::Role::kStaticText) {
    AXNode* node = tree_->GetFromId(new_node_data.id);
    tree_events_[node].insert(
        EventParams(Event::CHILDREN_CHANGED, ax::mojom::EventFrom::kNone));
  }
}

void AXEventGenerator::OnRoleChanged(AXTree* tree,
                                     AXNode* node,
                                     ax::mojom::Role old_role,
                                     ax::mojom::Role new_role) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, Event::ROLE_CHANGED);
}

void AXEventGenerator::OnStateChanged(AXTree* tree,
                                      AXNode* node,
                                      ax::mojom::State state,
                                      bool new_value) {
  DCHECK_EQ(tree_, tree);

  AddEvent(node, Event::STATE_CHANGED);
  switch (state) {
    case ax::mojom::State::kExpanded:
      AddEvent(node, new_value ? Event::EXPANDED : Event::COLLAPSED);

      // TODO(accessibility): tree in the midst of updates. Disallow access to
      // |node|.
      if (node->data().role == ax::mojom::Role::kRow ||
          node->data().role == ax::mojom::Role::kTreeItem) {
        ui::AXNode* container = node;
        while (container && !ui::IsRowContainer(container->data().role))
          container = container->parent();
        if (container)
          AddEvent(container, Event::ROW_COUNT_CHANGED);
      }
      break;
    case ax::mojom::State::kIgnored: {
      ui::AXNode* unignored_parent = node->GetUnignoredParent();
      if (unignored_parent)
        AddEvent(unignored_parent, Event::CHILDREN_CHANGED);
      break;
    }
    default:
      break;
  }
}

void AXEventGenerator::OnStringAttributeChanged(AXTree* tree,
                                                AXNode* node,
                                                ax::mojom::StringAttribute attr,
                                                const std::string& old_value,
                                                const std::string& new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ax::mojom::StringAttribute::kName:
      // If the name of the root node changes, we expect OnTreeDataChanged to
      // add a DOCUMENT_TITLE_CHANGED event instead.
      if (node != tree->root())
        AddEvent(node, Event::NAME_CHANGED);

      // TODO(accessibility): tree in the midst of updates. Disallow
      // access to |node|.
      if (node->data().HasStringAttribute(
              ax::mojom::StringAttribute::kContainerLiveStatus)) {
        FireLiveRegionEvents(node);
      }
      break;
    case ax::mojom::StringAttribute::kDescription:
      AddEvent(node, Event::DESCRIPTION_CHANGED);
      break;
    case ax::mojom::StringAttribute::kValue:
      AddEvent(node, Event::VALUE_CHANGED);
      break;
    case ax::mojom::StringAttribute::kAriaInvalidValue:
      AddEvent(node, Event::INVALID_STATUS_CHANGED);
      break;
    case ax::mojom::StringAttribute::kLiveStatus:
      // TODO(accessibility): tree in the midst of updates. Disallow access to
      // |node|.
      if (node->data().role != ax::mojom::Role::kAlert)
        AddEvent(node, Event::LIVE_REGION_CREATED);
      break;
    default:
      AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
      break;
  }
}

void AXEventGenerator::OnIntAttributeChanged(AXTree* tree,
                                             AXNode* node,
                                             ax::mojom::IntAttribute attr,
                                             int32_t old_value,
                                             int32_t new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ax::mojom::IntAttribute::kActivedescendantId:
      // Don't fire on invisible containers, as it confuses some screen readers,
      // such as NVDA.
      if (!node->data().HasState(ax::mojom::State::kInvisible)) {
        AddEvent(node, Event::ACTIVE_DESCENDANT_CHANGED);
        active_descendant_changed_.push_back(node);
      }
      break;
    case ax::mojom::IntAttribute::kCheckedState:
      AddEvent(node, Event::CHECKED_STATE_CHANGED);
      break;
    case ax::mojom::IntAttribute::kInvalidState:
      AddEvent(node, Event::INVALID_STATUS_CHANGED);
      break;
    case ax::mojom::IntAttribute::kRestriction:
      AddEvent(node, Event::STATE_CHANGED);
      break;
    case ax::mojom::IntAttribute::kScrollX:
    case ax::mojom::IntAttribute::kScrollY:
      AddEvent(node, Event::SCROLL_POSITION_CHANGED);
      break;
    default:
      AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
      break;
  }
}

void AXEventGenerator::OnFloatAttributeChanged(AXTree* tree,
                                               AXNode* node,
                                               ax::mojom::FloatAttribute attr,
                                               float old_value,
                                               float new_value) {
  DCHECK_EQ(tree_, tree);

  if (attr == ax::mojom::FloatAttribute::kValueForRange)
    AddEvent(node, Event::VALUE_CHANGED);
  else
    AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnBoolAttributeChanged(AXTree* tree,
                                              AXNode* node,
                                              ax::mojom::BoolAttribute attr,
                                              bool new_value) {
  DCHECK_EQ(tree_, tree);

  if (attr == ax::mojom::BoolAttribute::kSelected) {
    AddEvent(node, Event::SELECTED_CHANGED);
    ui::AXNode* container = node;
    while (container &&
           !ui::IsContainerWithSelectableChildren(container->data().role))
      container = container->parent();
    if (container)
      AddEvent(container, Event::SELECTED_CHILDREN_CHANGED);
    return;
  }

  AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnIntListAttributeChanged(
    AXTree* tree,
    AXNode* node,
    ax::mojom::IntListAttribute attr,
    const std::vector<int32_t>& old_value,
    const std::vector<int32_t>& new_value) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnStringListAttributeChanged(
    AXTree* tree,
    AXNode* node,
    ax::mojom::StringListAttribute attr,
    const std::vector<std::string>& old_value,
    const std::vector<std::string>& new_value) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnTreeDataChanged(AXTree* tree,
                                         const ui::AXTreeData& old_tree_data,
                                         const ui::AXTreeData& new_tree_data) {
  DCHECK_EQ(tree_, tree);

  if (new_tree_data.loaded && !old_tree_data.loaded &&
      ShouldFireLoadEvents(tree->root())) {
    AddEvent(tree->root(), Event::LOAD_COMPLETE);
  }

  if (new_tree_data.sel_anchor_object_id !=
          old_tree_data.sel_anchor_object_id ||
      new_tree_data.sel_anchor_offset != old_tree_data.sel_anchor_offset ||
      new_tree_data.sel_anchor_affinity != old_tree_data.sel_anchor_affinity ||
      new_tree_data.sel_focus_object_id != old_tree_data.sel_focus_object_id ||
      new_tree_data.sel_focus_offset != old_tree_data.sel_focus_offset ||
      new_tree_data.sel_focus_affinity != old_tree_data.sel_focus_affinity) {
    AddEvent(tree->root(), Event::DOCUMENT_SELECTION_CHANGED);
  }
  if (new_tree_data.title != old_tree_data.title)
    AddEvent(tree->root(), Event::DOCUMENT_TITLE_CHANGED);
}

void AXEventGenerator::OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  tree_events_.erase(node);
}

void AXEventGenerator::OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  tree_events_.erase(node);
}

void AXEventGenerator::OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeCreated(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeChanged(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  DCHECK_EQ(tree_, tree);

  if (root_changed && ShouldFireLoadEvents(tree->root())) {
    if (tree->data().loaded)
      AddEvent(tree->root(), Event::LOAD_COMPLETE);
    else
      AddEvent(tree->root(), Event::LOAD_START);
  }

  for (const auto& change : changes) {
    if ((change.type == NODE_CREATED || change.type == SUBTREE_CREATED)) {
      if (change.node->data().HasStringAttribute(
              ax::mojom::StringAttribute::kLiveStatus)) {
        if (change.node->data().role == ax::mojom::Role::kAlert)
          AddEvent(change.node, Event::ALERT);
        else
          AddEvent(change.node, Event::LIVE_REGION_CREATED);
      } else if (change.node->data().HasStringAttribute(
                     ax::mojom::StringAttribute::kContainerLiveStatus) &&
                 change.node->data().HasStringAttribute(
                     ax::mojom::StringAttribute::kName)) {
        FireLiveRegionEvents(change.node);
      }
    }

    if (change.type != NODE_CREATED && change.type != SUBTREE_CREATED)
      FireRelationSourceEvents(tree, change.node);
  }

  FireActiveDescendantEvents();
}

void AXEventGenerator::FireLiveRegionEvents(AXNode* node) {
  ui::AXNode* live_root = node;
  while (live_root && !live_root->data().HasStringAttribute(
                          ax::mojom::StringAttribute::kLiveStatus))
    live_root = live_root->parent();

  if (live_root &&
      !live_root->data().GetBoolAttribute(ax::mojom::BoolAttribute::kBusy)) {
    // Fire LIVE_REGION_NODE_CHANGED on each node that changed.
    if (!node->data()
             .GetStringAttribute(ax::mojom::StringAttribute::kName)
             .empty())
      AddEvent(node, Event::LIVE_REGION_NODE_CHANGED);
    // Fire LIVE_REGION_NODE_CHANGED on the root of the live region.
    AddEvent(live_root, Event::LIVE_REGION_CHANGED);
  }
}

void AXEventGenerator::FireActiveDescendantEvents() {
  for (AXNode* node : active_descendant_changed_) {
    AXNode* descendant = tree_->GetFromId(node->data().GetIntAttribute(
        ax::mojom::IntAttribute::kActivedescendantId));
    if (!descendant)
      continue;
    switch (descendant->data().role) {
      case ax::mojom::Role::kMenuItem:
      case ax::mojom::Role::kMenuItemCheckBox:
      case ax::mojom::Role::kMenuItemRadio:
      case ax::mojom::Role::kMenuListOption:
        AddEvent(descendant, Event::MENU_ITEM_SELECTED);
        break;
      default:
        break;
    }
  }
  active_descendant_changed_.clear();
}

void AXEventGenerator::FireRelationSourceEvents(AXTree* tree,
                                                AXNode* target_node) {
  int32_t target_id = target_node->id();
  std::set<AXNode*> source_nodes;
  auto callback = [&](const auto& entry) {
    const auto& target_to_sources = entry.second;
    auto sources_it = target_to_sources.find(target_id);
    if (sources_it == target_to_sources.end())
      return;

    auto sources = sources_it->second;
    std::for_each(sources.begin(), sources.end(), [&](int32_t source_id) {
      AXNode* source_node = tree->GetFromId(source_id);

      if (!source_node || source_nodes.count(source_node) > 0)
        return;

      source_nodes.insert(source_node);

      // GCC < 6.4 requires this pointer when calling a member
      // function in anonymous function
      this->AddEvent(source_node, Event::RELATED_NODE_CHANGED);
    });
  };

  std::for_each(tree->int_reverse_relations().begin(),
                tree->int_reverse_relations().end(), callback);
  std::for_each(tree->intlist_reverse_relations().begin(),
                tree->intlist_reverse_relations().end(), callback);
}

// Attempts to suppress load-related events that we presume no AT will be
// interested in under any circumstances, such as pages which have no size.
bool AXEventGenerator::ShouldFireLoadEvents(AXNode* node) {
  const AXNodeData& data = node->data();
  return data.location.width() || data.location.height();
}

}  // namespace ui
