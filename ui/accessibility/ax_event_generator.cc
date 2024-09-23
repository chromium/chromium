// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

namespace {

bool HasEvent(const std::set<AXEventGenerator::EventParams>& node_events,
              AXEventGenerator::Event event) {
  return node_events.count(AXEventGenerator::EventParams(event));
}

void RemoveEvent(std::set<AXEventGenerator::EventParams>* node_events,
                 AXEventGenerator::Event event) {
  node_events->erase(AXEventGenerator::EventParams(event));
}

// If a node toggled its ignored state, don't also fire children-changed because
// platforms likely will do that in response to ignored-changed. Also do not
// fire parent-changed on ignored nodes because functionally the parent did not
// change as far as platform assistive technologies are concerned.
// Suppress name- and description-changed because those can be emitted as a side
// effect of calculating alternative text values for a newly-displayed object.
// Ditto for text attributes such as foreground and background colors, or
// display changing from "none" to "block."
void RemoveEventsDueToIgnoredChanged(
    std::set<AXEventGenerator::EventParams>* node_events) {
  RemoveEvent(node_events,
              AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::CHILDREN_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::DESCRIPTION_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::NAME_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::PARENT_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::SORT_CHANGED);
  RemoveEvent(node_events, AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED);
  RemoveEvent(node_events,
              AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED);
}

// Add a particular AXEventGenerator::IgnoredChangedState to
// |ignored_changed_states|.
void AddIgnoredChangedState(
    const AXEventGenerator::IgnoredChangedState& state,
    AXEventGenerator::IgnoredChangedStatesBitset* ignored_changed_states) {
  ignored_changed_states->set(static_cast<size_t>(state));
}

// Returns true if |ignored_changed_states| contains a particular
// AXEventGenerator::IgnoredChangedState.
bool HasIgnoredChangedState(
    const AXEventGenerator::IgnoredChangedStatesBitset& ignored_changed_states,
    const AXEventGenerator::IgnoredChangedState& state) {
  return ignored_changed_states[static_cast<size_t>(state)];
}

}  // namespace

//
// AXEventGenerator::EventParams
//

AXEventGenerator::EventParams::EventParams(const Event event) : event(event) {}

AXEventGenerator::EventParams::EventParams(
    const Event event,
    const ax::mojom::EventFrom event_from,
    const ax::mojom::Action event_from_action,
    const std::vector<AXEventIntent>& event_intents)
    : event(event),
      event_from(event_from),
      event_from_action(event_from_action),
      event_intents(event_intents) {}

AXEventGenerator::EventParams::EventParams(const EventParams& other) = default;

AXEventGenerator::EventParams::~EventParams() = default;

AXEventGenerator::EventParams& AXEventGenerator::EventParams::operator=(
    const EventParams& other) = default;

bool AXEventGenerator::EventParams::operator==(const EventParams& rhs) const {
  return rhs.event == event;
}

bool AXEventGenerator::EventParams::operator<(const EventParams& rhs) const {
  return event < rhs.event;
}

//
// AXEventGenerator::TargetedEvent
//

AXEventGenerator::TargetedEvent::TargetedEvent(AXNodeID node_id,
                                               const EventParams& event_params)
    : node_id(node_id), event_params(event_params) {
  DCHECK_NE(node_id, kInvalidAXNodeID);
}

AXEventGenerator::TargetedEvent::~TargetedEvent() = default;

//
// AXEventGenerator::Iterator
//

AXEventGenerator::Iterator::Iterator(
    std::map<AXNodeID, std::set<EventParams>>::const_iterator map_start_iter,
    std::map<AXNodeID, std::set<EventParams>>::const_iterator map_end_iter)
    : map_iter_(map_start_iter), map_end_iter_(map_end_iter) {
  if (map_iter_ != map_end_iter_)
    set_iter_ = map_iter_->second.begin();
}

AXEventGenerator::Iterator::Iterator(const AXEventGenerator::Iterator& other) =
    default;

AXEventGenerator::Iterator::~Iterator() = default;

AXEventGenerator::Iterator& AXEventGenerator::Iterator::operator=(
    const Iterator& other) = default;

AXEventGenerator::Iterator& AXEventGenerator::Iterator::operator++() {
  if (map_iter_ == map_end_iter_)
    return *this;

  CHECK(set_iter_ != map_iter_->second.end(), base::NotFatalUntil::M130);
  set_iter_++;

  // The map pointed to by |map_end_iter_| may contain empty sets of events in
  // its entries (i.e. |set_iter_| is at the iterator's end). In this case, we
  // want to increment |map_iter_| to point to the next entry of the map that
  // contains a non-empty set of events.
  while (map_iter_ != map_end_iter_ && set_iter_ == map_iter_->second.end()) {
    map_iter_++;
    if (map_iter_ != map_end_iter_)
      set_iter_ = map_iter_->second.begin();
  }

  return *this;
}

AXEventGenerator::Iterator AXEventGenerator::Iterator::operator++(int) {
  if (map_iter_ == map_end_iter_)
    return *this;
  Iterator iter = *this;
  ++(*this);
  return iter;
}

AXEventGenerator::TargetedEvent AXEventGenerator::Iterator::operator*() const {
  DCHECK(map_iter_ != map_end_iter_);
  CHECK(set_iter_ != map_iter_->second.end(), base::NotFatalUntil::M130);
  return AXEventGenerator::TargetedEvent(map_iter_->first, *set_iter_);
}

bool operator==(const AXEventGenerator::Iterator& lhs,
                const AXEventGenerator::Iterator& rhs) {
  if (lhs.map_iter_ == lhs.map_end_iter_ && rhs.map_iter_ == rhs.map_end_iter_)
    return true;
  return lhs.map_iter_ == rhs.map_iter_ && lhs.set_iter_ == rhs.set_iter_;
}

bool operator!=(const AXEventGenerator::Iterator& lhs,
                const AXEventGenerator::Iterator& rhs) {
  return !(lhs == rhs);
}

void swap(AXEventGenerator::Iterator& lhs, AXEventGenerator::Iterator& rhs) {
  if (lhs == rhs)
    return;

  std::map<AXNodeID, std::set<AXEventGenerator::EventParams>>::const_iterator
      map_iter = lhs.map_iter_;
  lhs.map_iter_ = rhs.map_iter_;
  rhs.map_iter_ = map_iter;
  std::map<AXNodeID, std::set<AXEventGenerator::EventParams>>::const_iterator
      map_end_iter = lhs.map_end_iter_;
  lhs.map_end_iter_ = rhs.map_end_iter_;
  rhs.map_end_iter_ = map_end_iter;
  std::set<AXEventGenerator::EventParams>::const_iterator set_iter =
      lhs.set_iter_;
  lhs.set_iter_ = rhs.set_iter_;
  rhs.set_iter_ = set_iter;
}

//
// AXEventGenerator
//

AXEventGenerator::AXEventGenerator() = default;

AXEventGenerator::AXEventGenerator(AXTree* tree) : tree_(tree) {
  if (tree)  // Can be null in unit tests.
    tree_event_observation_.Observe(tree_.get());
}

AXEventGenerator::~AXEventGenerator() = default;

void AXEventGenerator::SetTree(AXTree* new_tree) {
  if (tree_) {
    DCHECK(tree_event_observation_.IsObservingSource(tree_.get()));
    tree_event_observation_.Reset();
  }
  tree_ = new_tree;
  if (tree_)
    tree_event_observation_.Observe(tree_.get());
}

void AXEventGenerator::ReleaseTree() {
  tree_event_observation_.Reset();
  tree_ = nullptr;
}

bool AXEventGenerator::empty() const {
  return tree_events_.empty();
}

size_t AXEventGenerator::size() const {
  return tree_events_.size();
}

AXEventGenerator::Iterator AXEventGenerator::begin() const {
  auto map_iter = tree_events_.begin();
  if (map_iter != tree_events_.end()) {
    auto set_iter = map_iter->second.begin();

    // |tree_events_| may contain empty sets of events in its first entry
    // (i.e. |set_iter| is at the iterator's end). In this case, we want to
    // increment |map_iter| to point to the next entry of |tree_events_| that
    // contains a non-empty set of events.
    while (map_iter != tree_events_.end() &&
           set_iter == map_iter->second.end()) {
      map_iter++;
      if (map_iter != tree_events_.end())
        set_iter = map_iter->second.begin();
    }
  }

  return AXEventGenerator::Iterator(map_iter, tree_events_.end());
}

AXEventGenerator::Iterator AXEventGenerator::end() const {
  return AXEventGenerator::Iterator(tree_events_.end(), tree_events_.end());
}

void AXEventGenerator::ClearEvents() {
  tree_events_.clear();
}

void AXEventGenerator::AddEvent(AXNode* node, AXEventGenerator::Event event) {
  DCHECK(node);

  if (node->GetRole() == ax::mojom::Role::kInlineTextBox)
    return;

  // Extra Mac node creation and deletion in `AXTableInfo` directly call AXTree
  // observer methods, which skips all unserialization logic found in
  // `AXTree::Unserialize`.
  //
  // It only makes sense to generate events when we are called here within
  // `AXTree::Unserialize`. The below condition also guards against any future
  // callers of this type, whether Mac or not.
  if (!tree_->event_data())
    return;

  std::set<EventParams>& node_events = tree_events_[node->id()];
  node_events.emplace(event, tree_->event_data()->event_from,
                      tree_->event_data()->event_from_action,
                      tree_->event_data()->event_intents);
}

void AXEventGenerator::RegisterEventOnNode(Event event_type, AXNodeID node_id) {
  registered_event_to_node_ids_[event_type].insert(node_id);
}

void AXEventGenerator::UnregisterEventOnNode(Event event_type,
                                             AXNodeID node_id) {
  auto it = registered_event_to_node_ids_.find(event_type);
  if (it == registered_event_to_node_ids_.end()) {
    return;
  }
  registered_event_to_node_ids_[event_type].erase(node_id);
  if (registered_event_to_node_ids_[event_type].empty()) {
    registered_event_to_node_ids_.erase(event_type);
  }
}

void AXEventGenerator::OnIgnoredWillChange(
    AXTree* tree,
    AXNode* node,
    bool is_ignored_new_value,
    bool is_changing_unignored_parents_children) {
  // If the node had been ignored and invisible before it changes to unignored,
  // then we should not fire `EVENT::PARENT_CHANGED` on its children because
  // they were previously unknown to ATs as they were in a hidden subtree.
  // TODO(nektar): Handle this by instead removing the `PARENT_CHANGED` event in
  // post-processing, because invisibility is not an accurate indicator of
  // whether nodes are known to the AT. In fact, a node can be invisible and
  // have visible descendants.
  if (!is_ignored_new_value && node->data().IsInvisible())
    nodes_to_suppress_parent_changed_on_.insert(node->id());
}

void AXEventGenerator::OnNodeDataChanged(AXTree* tree,
                                         const AXNodeData& old_node_data,
                                         const AXNodeData& new_node_data) {
  DCHECK_EQ(tree_, tree);
  AXNode* node = tree_->GetFromId(new_node_data.id);
  if (!node)
    return;

  // We can't rely on `AttributeChanged` methods since ARIA notifications may be
  // posted more than once with the same announcement and properties.
  // Each `AriaNotification` is discarded once it's serialized, so we can simply
  // check if there's notification data; it won't persist across updates.
  //
  // TODO(crbug.com/330589205): Remove all of this when we develop a better way
  // to send single-use data attached to events. DO NOT replicate this pattern,
  // as this method is not intended to check for attribute changes.
  if (new_node_data.HasStringListAttribute(
          ax::mojom::StringListAttribute::kAriaNotificationAnnouncements)) {
    AddEvent(node, Event::ARIA_NOTIFICATIONS_POSTED);
  }

  // Internally we store inline text box nodes as children of a static text
  // node or a line break node, which enables us to determine character bounds
  // and line layout. We don't expose those to platform APIs, though, so
  // suppress CHILDREN_CHANGED events on static text nodes.
  if (node->IsText()) {
    return;
  }

  bool was_ignored = old_node_data.IsIgnored();
  bool is_ignored = new_node_data.IsIgnored();

  if (was_ignored != is_ignored) {
    // The ignored state is changing, which cause the current node to be added
    // or removed to the unignored ancestor. Therefore, a children changed event
    // must be fired on the unignored ancestor. The node data may not have
    // changed in this case, because the ignored state can change based on
    // focus, which is in tree data. Therefore, firing Event::CHILDREN_CHANGED
    // when ignored changes is is handled in OnIgnoredChanged().
    return;
  }

  if (new_node_data.child_ids == old_node_data.child_ids) {
    // If the child ids remained the same, then children changed doesn't need to
    // be fired.
    return;
  }

  if (is_ignored) {
    // Node remained ignored. If its child ids changed, that means
    // the unignored parent's children changed.
    AXNode* unignored_parent = node->GetUnignoredParent();
    DUMP_WILL_BE_CHECK(unignored_parent)
        << "The root cannot be ignored, so an unignored parent is always "
           "found.";
    AddEvent(unignored_parent, Event::CHILDREN_CHANGED);
  } else {
    // Node remained unignored. Fire a children changed event on it.
    AddEvent(node, Event::CHILDREN_CHANGED);
  }
}

void AXEventGenerator::OnRoleChanged(AXTree* tree,
                                     AXNode* node,
                                     ax::mojom::Role old_role,
                                     ax::mojom::Role new_role) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, new_role == ax::mojom::Role::kAlert ? Event::ALERT
                                                     : Event::ROLE_CHANGED);
}

void AXEventGenerator::OnIgnoredChanged(AXTree* tree,
                                        AXNode* node,
                                        bool is_ignored_new_value) {
  DCHECK_EQ(tree_, tree);

  AXNode* unignored_parent = node->GetUnignoredParent();
  DUMP_WILL_BE_CHECK(unignored_parent)
      << "The root cannot be ignored, so an unignored parent is always "
         "found.";
  AddEvent(unignored_parent, Event::CHILDREN_CHANGED);

  AddEvent(node, Event::IGNORED_CHANGED);
  if (!is_ignored_new_value)
    AddEvent(node, Event::SUBTREE_CREATED);
  if (node->GetRole() == ax::mojom::Role::kMenu) {
    if (is_ignored_new_value) {
      AddEvent(node, Event::MENU_POPUP_END);
    } else {
      AddEvent(node, Event::MENU_POPUP_START);
    }
  }

  // If the ignored state of a node has changed, the inclusion/exclusion of that
  // node in platform accessibility trees will change. Fire PARENT_CHANGED on
  // the unignored children of a node whose ignored state changed in order to
  // notify ATs that existing children may have been reparented.
  //
  // Note that we should not fire parent-changed if the invisible state of the
  // node has changed because when invisibility changes, the entire subtree is
  // being inserted / removed. For example if the 'hidden' property is changed
  // on list item, we should not fire parent-changed on the list marker or
  // static text children because: A) If the 'hidden' property has been removed,
  // both the list marker and the static text were not previously known to ATs
  // since they were not in the accessibility tree, and B) If the 'hidden'
  // property has been added then the children will be ignored and thus no
  // longer be in the accessibility tree that is exposed to the platform layer.
  // The second condition is automatically taken care of by the fact that we are
  // only iterating through unignored children.
  // TODO(nektar): Handle this by instead removing the `PARENT_CHANGED` event in
  // post-processing, because invisibility is not an accurate indicator of
  // whether nodes are known to the AT. In fact, a node can be invisible and
  // have visible descendants.
  const bool was_in_invisible_subtree =
      !base::Contains(nodes_to_suppress_parent_changed_on_, node->id());
  if (was_in_invisible_subtree) {
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      AddEvent(iter.get(), Event::PARENT_CHANGED);
    }
  }
}

void AXEventGenerator::OnStateChanged(AXTree* tree,
                                      AXNode* node,
                                      ax::mojom::State state,
                                      bool new_value) {
  DCHECK_EQ(tree_, tree);
  DCHECK_NE(state, ax::mojom::State::kIgnored)
      << "The ignored state should be handled in "
         "`AXEventGenerator::OnIgnoredChanged` and not in this method.";
  if (node->IsIgnored())
    return;
  AddEvent(node, Event::STATE_CHANGED);
  AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);

  switch (state) {
    case ax::mojom::State::kExpanded:
      if (node->data().HasState(ax::mojom::State::kCollapsed) ||
          node->data().HasState(ax::mojom::State::kExpanded)) {
        AddEvent(node, new_value ? Event::EXPANDED : Event::COLLAPSED);
      }
      if (IsTableRow(node->GetRole()) ||
          node->GetRole() == ax::mojom::Role::kTreeItem) {
        AXNode* container = node;
        while (container && !IsRowContainer(container->GetRole()))
          container = container->parent();
        if (container)
          AddEvent(container, Event::ROW_COUNT_CHANGED);
      }
      break;
    case ax::mojom::State::kMultiline:
      AddEvent(node, Event::MULTILINE_STATE_CHANGED);
      break;
    case ax::mojom::State::kMultiselectable:
      AddEvent(node, Event::MULTISELECTABLE_STATE_CHANGED);
      break;
    case ax::mojom::State::kRequired:
      AddEvent(node, Event::REQUIRED_STATE_CHANGED);
      break;
    case ax::mojom::State::kAutofillAvailable:
      AddEvent(node, Event::AUTOFILL_AVAILABILITY_CHANGED);
      break;
    case ax::mojom::State::kHorizontal:
    case ax::mojom::State::kVertical:
      AddEvent(node, Event::ORIENTATION_CHANGED);
      break;
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
    case ax::mojom::StringAttribute::kAccessKey:
      AddEvent(node, Event::ACCESS_KEY_CHANGED);
      break;
    case ax::mojom::StringAttribute::kAutoComplete:
      AddEvent(node, Event::AUTO_COMPLETE_CHANGED);
      break;
    case ax::mojom::StringAttribute::kCheckedStateDescription:
      AddEvent(node, Event::CHECKED_STATE_DESCRIPTION_CHANGED);
      break;
    case ax::mojom::StringAttribute::kClassName:
      break;
    case ax::mojom::StringAttribute::kDescription:
      AddEvent(node, Event::DESCRIPTION_CHANGED);
      break;
    case ax::mojom::StringAttribute::kFontFamily:
      if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::TEXT_ATTRIBUTE_CHANGED);
      }
      break;
    case ax::mojom::StringAttribute::kImageAnnotation:
      // The image annotation is reported as part of the accessible name.
      AddEvent(node, Event::IMAGE_ANNOTATION_CHANGED);
      break;
    case ax::mojom::StringAttribute::kKeyShortcuts:
      AddEvent(node, Event::KEY_SHORTCUTS_CHANGED);
      break;
    case ax::mojom::StringAttribute::kLanguage:
      AddEvent(node, Event::LANGUAGE_CHANGED);
      break;
    case ax::mojom::StringAttribute::kLiveRelevant:
      AddEvent(node, Event::LIVE_RELEVANT_CHANGED);
      break;
    case ax::mojom::StringAttribute::kLiveStatus:
      AddEvent(node, Event::LIVE_STATUS_CHANGED);

      // Fire a LIVE_REGION_CREATED if the previous value was off, and the new
      // value is not-off. According to the ARIA spec, "When the property is not
      // set on an object that needs to send updates, the politeness level is
      // the value of the nearest ancestor that sets the aria-live attribute."
      // Example: A new chat message is added to the room with aria-live="off",
      // then the author removes aria-live.
      if (!IsAlert(node->GetRole())) {
        bool was_off = !old_value.empty()
                           ? old_value == "off"
                           : !node->data().IsContainedInActiveLiveRegion();
        bool is_off = !new_value.empty()
                          ? new_value == "off"
                          : !node->data().IsContainedInActiveLiveRegion();
        if (was_off && !is_off)
          AddEvent(node, Event::LIVE_REGION_CREATED);
      }
      break;
    case ax::mojom::StringAttribute::kName:
      // If the name of the root node changes, we expect OnTreeDataChanged to
      // add a DOCUMENT_TITLE_CHANGED event instead.
      if (node != tree->root())
        AddEvent(node, Event::NAME_CHANGED);

      // If it's in a live region, fire live region events.
      if (node->HasStringAttribute(
              ax::mojom::StringAttribute::kContainerLiveStatus)) {
        FireLiveRegionEvents(node, /* is_removal */ false);
      }

      FireValueInTextFieldChangedEventIfNecessary(tree, node);
      break;
    case ax::mojom::StringAttribute::kPlaceholder:
      AddEvent(node, Event::PLACEHOLDER_CHANGED);
      break;
    case ax::mojom::StringAttribute::kValue:
      if (node->data().IsRangeValueSupported()) {
        AddEvent(node, Event::RANGE_VALUE_CHANGED);
      } else if (IsSelectElement(node->GetRole())) {
        AddEvent(node, Event::SELECTED_VALUE_CHANGED);
      } else if (node->data().IsTextField()) {
        AddEvent(node, Event::VALUE_IN_TEXT_FIELD_CHANGED);
      }
      break;
    default:
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
      if (!node->data().IsInvisible()) {
        AddEvent(node, Event::ACTIVE_DESCENDANT_CHANGED);
        active_descendant_changed_.push_back(node);
      }
      break;
    case ax::mojom::IntAttribute::kCheckedState:
      AddEvent(node, Event::CHECKED_STATE_CHANGED);
      AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      break;
    case ax::mojom::IntAttribute::kAriaCurrentState:
      AddEvent(node, Event::ARIA_CURRENT_CHANGED);
      break;
    case ax::mojom::IntAttribute::kHasPopup:
      AddEvent(node, Event::HASPOPUP_CHANGED);
      AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      break;
    case ax::mojom::IntAttribute::kHierarchicalLevel:
      AddEvent(node, Event::HIERARCHICAL_LEVEL_CHANGED);
      break;
    case ax::mojom::IntAttribute::kInvalidState:
      AddEvent(node, Event::INVALID_STATUS_CHANGED);
      break;
    case ax::mojom::IntAttribute::kPosInSet:
      AddEvent(node, Event::POSITION_IN_SET_CHANGED);
      break;
    case ax::mojom::IntAttribute::kRestriction: {
      bool was_enabled;
      bool was_readonly;
      GetRestrictionStates(static_cast<ax::mojom::Restriction>(old_value),
                           &was_enabled, &was_readonly);
      bool is_enabled;
      bool is_readonly;
      GetRestrictionStates(static_cast<ax::mojom::Restriction>(new_value),
                           &is_enabled, &is_readonly);

      if (was_enabled != is_enabled) {
        AddEvent(node, Event::ENABLED_CHANGED);
        AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      }
      if (was_readonly != is_readonly) {
        AddEvent(node, Event::READONLY_CHANGED);
        AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      }
      break;
    }
    case ax::mojom::IntAttribute::kScrollX:
      AddEvent(node, Event::SCROLL_HORIZONTAL_POSITION_CHANGED);
      break;
    case ax::mojom::IntAttribute::kScrollY:
      AddEvent(node, Event::SCROLL_VERTICAL_POSITION_CHANGED);
      break;
    case ax::mojom::IntAttribute::kSortDirection:
      // Ignore sort direction changes on roles other than table headers and
      // grid headers.
      if (IsTableHeader(node->GetRole()))
        AddEvent(node, Event::SORT_CHANGED);
      break;
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
      // The image annotation is reported as part of the accessible name.
      AddEvent(node, Event::IMAGE_ANNOTATION_CHANGED);
      break;
    case ax::mojom::IntAttribute::kSetSize:
      AddEvent(node, Event::SET_SIZE_CHANGED);
      break;
    case ax::mojom::IntAttribute::kBackgroundColor:
    case ax::mojom::IntAttribute::kColor:
    case ax::mojom::IntAttribute::kTextDirection:
    case ax::mojom::IntAttribute::kTextPosition:
    case ax::mojom::IntAttribute::kTextStyle:
    case ax::mojom::IntAttribute::kTextOverlineStyle:
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
      if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::TEXT_ATTRIBUTE_CHANGED);
      }
      break;
    case ax::mojom::IntAttribute::kTextAlign:
      // Alignment is exposed as an object attribute because it cannot apply to
      // a substring. However, for some platforms (e.g. ATK), alignment is a
      // text attribute. Therefore fire both events to ensure platforms get the
      // expected notifications.
      if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED);
        AddEvent(node, Event::OBJECT_ATTRIBUTE_CHANGED);
      }
      break;
    default:
      break;
  }
}

void AXEventGenerator::OnFloatAttributeChanged(AXTree* tree,
                                               AXNode* node,
                                               ax::mojom::FloatAttribute attr,
                                               float old_value,
                                               float new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ax::mojom::FloatAttribute::kMaxValueForRange:
      AddEvent(node, Event::RANGE_VALUE_MAX_CHANGED);
      break;
    case ax::mojom::FloatAttribute::kMinValueForRange:
      AddEvent(node, Event::RANGE_VALUE_MIN_CHANGED);
      break;
    case ax::mojom::FloatAttribute::kStepValueForRange:
      AddEvent(node, Event::RANGE_VALUE_STEP_CHANGED);
      break;
    case ax::mojom::FloatAttribute::kValueForRange:
      AddEvent(node, Event::RANGE_VALUE_CHANGED);
      break;
    case ax::mojom::FloatAttribute::kFontSize:
    case ax::mojom::FloatAttribute::kFontWeight:
      if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::TEXT_ATTRIBUTE_CHANGED);
      }
      break;
    case ax::mojom::FloatAttribute::kTextIndent:
      // Indentation is exposed as an object attribute because it cannot apply
      // to a substring. However, for some platforms (e.g. ATK), alignment is a
      // text attribute. Therefore fire both events to ensure platforms get the
      // expected notifications.
      if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED);
        AddEvent(node, Event::OBJECT_ATTRIBUTE_CHANGED);
      }
      break;
    default:
      break;
  }
}

void AXEventGenerator::OnBoolAttributeChanged(AXTree* tree,
                                              AXNode* node,
                                              ax::mojom::BoolAttribute attr,
                                              bool new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ax::mojom::BoolAttribute::kBusy:
      AddEvent(node, Event::BUSY_CHANGED);
      AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      // Fire an 'invalidated' event when aria-busy becomes false.
      if (!new_value)
        AddEvent(node, Event::LAYOUT_INVALIDATED);
      break;
    case ax::mojom::BoolAttribute::kLiveAtomic:
      AddEvent(node, Event::ATOMIC_CHANGED);
      break;
    case ax::mojom::BoolAttribute::kSelected: {
      AddEvent(node, Event::SELECTED_CHANGED);
      AddEvent(node, Event::WIN_IACCESSIBLE_STATE_CHANGED);
      AXNode* container = node;
      while (container &&
             !IsContainerWithSelectableChildren(container->GetRole()))
        container = container->parent();
      if (container)
        AddEvent(container, Event::SELECTED_CHILDREN_CHANGED);
      break;
    }
    default:
      break;
  }
}

void AXEventGenerator::OnIntListAttributeChanged(
    AXTree* tree,
    AXNode* node,
    ax::mojom::IntListAttribute attr,
    const std::vector<int32_t>& old_value,
    const std::vector<int32_t>& new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ax::mojom::IntListAttribute::kControlsIds:
      AddEvent(node, Event::CONTROLS_CHANGED);
      break;
    case ax::mojom::IntListAttribute::kDetailsIds:
      AddEvent(node, Event::DETAILS_CHANGED);
      break;
    case ax::mojom::IntListAttribute::kDescribedbyIds:
      AddEvent(node, Event::DESCRIBED_BY_CHANGED);
      break;
    case ax::mojom::IntListAttribute::kFlowtoIds: {
      AddEvent(node, Event::FLOW_TO_CHANGED);

      // Fire FLOW_FROM_CHANGED for all nodes added or removed.
      // TODO(nektar): Consider removing this because flowto is unused by AT.
      for (AXNodeID id : ComputeIntListDifference(old_value, new_value)) {
        if (AXNode* target_node = tree->GetFromId(id))
          AddEvent(target_node, Event::FLOW_FROM_CHANGED);
      }
      break;
    }
    case ax::mojom::IntListAttribute::kLabelledbyIds:
      AddEvent(node, Event::LABELED_BY_CHANGED);
      break;
    case ax::mojom::IntListAttribute::kMarkerEnds:
    case ax::mojom::IntListAttribute::kMarkerStarts:
    case ax::mojom::IntListAttribute::kMarkerTypes:
      // On a native text field, the spelling- and grammar-error markers are
      // associated with children not exposed on any platform. Therefore, we
      // adjust the node we fire that event on here.
      if (AXNode* text_field = node->GetTextFieldAncestor()) {
        AddEvent(text_field, Event::TEXT_ATTRIBUTE_CHANGED);
      } else if (node->HasState(ax::mojom::State::kRichlyEditable)) {
        AddEvent(node, Event::TEXT_ATTRIBUTE_CHANGED);
      }
      break;
    case ax::mojom::IntListAttribute::kCaretBounds:
      AddEvent(node, Event::CARET_BOUNDS_CHANGED);
      break;
    default:
      break;
  }
}

void AXEventGenerator::OnTreeDataChanged(AXTree* tree,
                                         const AXTreeData& old_tree_data,
                                         const AXTreeData& new_tree_data) {
  DCHECK_EQ(tree_, tree);
  DCHECK(tree->root());

  if (new_tree_data.title != old_tree_data.title)
    AddEvent(tree->root(), Event::DOCUMENT_TITLE_CHANGED);

  if (new_tree_data.sel_is_backward != old_tree_data.sel_is_backward ||
      new_tree_data.sel_anchor_object_id !=
          old_tree_data.sel_anchor_object_id ||
      new_tree_data.sel_anchor_offset != old_tree_data.sel_anchor_offset ||
      new_tree_data.sel_anchor_affinity != old_tree_data.sel_anchor_affinity ||
      new_tree_data.sel_focus_object_id != old_tree_data.sel_focus_object_id ||
      new_tree_data.sel_focus_offset != old_tree_data.sel_focus_offset ||
      new_tree_data.sel_focus_affinity != old_tree_data.sel_focus_affinity) {
    AddEvent(tree->root(), Event::DOCUMENT_SELECTION_CHANGED);

    // A special event is also fired internally for selection changes in text
    // fields. The reasons are both historical and in order to have a unified
    // way of handling selection changes between Web and Views. Views don't have
    // the concept of a document selection but some individual Views controls
    // have the ability for the user to select text inside them.
    const AXNode* selection_focus =
        tree_->GetFromId(new_tree_data.sel_focus_object_id);
    if (selection_focus) {
      // Even if it is possible for the document selection to span multiple text
      // fields, an event should still fire on the field where the selection
      // ends.
      if (AXNode* text_field = selection_focus->GetTextFieldAncestor())
        AddEvent(text_field, Event::TEXT_SELECTION_CHANGED);
    }
  }
}

void AXEventGenerator::OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  FireValueInTextFieldChangedEventIfNecessary(tree, node);
  FireLiveRegionEvents(node, /* removal */ true);
}

void AXEventGenerator::OnNodeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeDeleted(AXTree* tree, AXNodeID node_id) {
  DCHECK_EQ(tree_, tree);
  tree_events_.erase(node_id);
}

void AXEventGenerator::OnNodeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, Event::PARENT_CHANGED);
}

void AXEventGenerator::OnNodeCreated(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  // Note: now that AXEventGenerator is part of AXTreeManager, this is being
  // called before BrowserAccessibilityManager::OnNodeCreated() is called,
  // where things used to be the other way around. That means that the new
  // node is in the tree's map, but not BAM's map yet, which means certain
  // calls, such as IsLeaf() may trigger a DCHECK because they call GetFromID(),
  // which checks to make sure that the id maps are in sync.
  // TODO(accesibility) Use a single id map so that issues like this go away.
  // Or for now, just have this call BAM::OnNodeCreated() directly to enforce
  // the order.
  FireValueInTextFieldChangedEventIfNecessary(tree, node);
}

void AXEventGenerator::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  DCHECK_EQ(tree_, tree);
  DCHECK(tree->root());

  for (const auto& change : changes) {
    DCHECK(change.node);

    if (change.type == SUBTREE_CREATED) {
      AddEvent(change.node, Event::SUBTREE_CREATED);
    } else if (change.type != NODE_CREATED) {
      FireRelationSourceEvents(tree, change.node);
      continue;
    }

    if (change.node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
        (change.type == SUBTREE_CREATED || change.type == NODE_CREATED)) {
      OnBoolAttributeChanged(tree, change.node,
                             ax::mojom::BoolAttribute::kSelected,
                             /*new_value*/ true);
    }

    if (IsAlert(change.node->GetRole()))
      AddEvent(change.node, Event::ALERT);
    else if (change.node->data().IsActiveLiveRegionRoot())
      AddEvent(change.node, Event::LIVE_REGION_CREATED);
    else if (change.node->data().IsContainedInActiveLiveRegion())
      FireLiveRegionEvents(change.node, /* is_removal */ false);
  }

  FireActiveDescendantEvents();
  nodes_to_suppress_parent_changed_on_.clear();

  PostprocessEvents();
}

void AXEventGenerator::AddEventsForTesting(
    const AXNode& node,
    const std::set<EventParams>& events) {
  tree_events_[node.id()] = events;
}

bool AXEventGenerator::IsRemovalRelevantInLiveRegion(AXNode* node) {
  std::string aria_relevant = node->GetStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant);
  if (aria_relevant.empty())
    return false;
  std::vector<std::string> tokens = base::SplitString(
      aria_relevant, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::any_of(tokens.begin(), tokens.end(), [](std::string token) {
    return token == "all" || token == "removals";
  });
}

void AXEventGenerator::FireLiveRegionEvents(AXNode* node, bool is_removal) {
  AXNode* live_root = node;
  std::string container_live = node->GetStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus);
  // Return early if not in a live region.
  if (container_live.empty() || container_live == "off")
    return;

  if (is_removal && !IsRemovalRelevantInLiveRegion(node))
    return;

  while (live_root &&
         live_root->GetStringAttribute(
             ax::mojom::StringAttribute::kLiveStatus) != container_live)
    live_root = live_root->parent();

  if (live_root &&
      !live_root->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy)) {
    // Fire LIVE_REGION_NODE_CHANGED on each node that changed.
    if (!node->GetStringAttribute(ax::mojom::StringAttribute::kName).empty())
      AddEvent(node, Event::LIVE_REGION_NODE_CHANGED);
    // Fire LIVE_REGION_NODE_CHANGED on the root of the live region.
    AddEvent(live_root, Event::LIVE_REGION_CHANGED);
  }
}

void AXEventGenerator::FireActiveDescendantEvents() {
  for (AXNode* node : active_descendant_changed_) {
    AXNode* descendant = tree_->GetFromId(
        node->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId));
    if (!descendant)
      continue;
    switch (descendant->GetRole()) {
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

bool CanContributeToValueOfTextfield(AXNode* target_node) {
  // Inline textboxes contribute the same text as their static text ancestors,
  // and their redundant contributions can be ignored to improve performance.
  if (target_node->GetRole() == ax::mojom::Role::kInlineTextBox) {
    return false;
  }

  // Text and line breaks contribute.
  if (IsText(target_node->GetRole())) {
    return true;
  }

  // Non-text leaf nodes contribute, e.g. images.
  if (target_node->GetChildCount() == 0) {
    return true;
  }

  return false;
}

void AXEventGenerator::FireValueInTextFieldChangedEventIfNecessary(
    AXTree* tree,
    AXNode* target_node) {
  if (!CanContributeToValueOfTextfield(target_node))
    return;

  AXNode* text_field_ancestor = target_node->GetTextFieldAncestor();
  if (!text_field_ancestor || text_field_ancestor == target_node)
    return;

  AddEvent(text_field_ancestor, Event::EDITABLE_TEXT_CHANGED);
  AddEvent(text_field_ancestor, Event::VALUE_IN_TEXT_FIELD_CHANGED);
}

void AXEventGenerator::FireRelationSourceEvents(AXTree* tree,
                                                AXNode* target_node) {
  auto it = registered_event_to_node_ids_.find(Event::RELATED_NODE_CHANGED);
  if (it == registered_event_to_node_ids_.end()) {
    return;
  }

  AXNode* registered_node = target_node;
  while (registered_node) {
    if (it->second.contains(registered_node->id())) {
      break;
    }
    registered_node = registered_node->parent();
  }
  if (!registered_node) {
    return;
  }

  AXNodeID target_id = target_node->id();
  std::set<AXNode*> source_nodes;
  auto callback = [&](const auto& entry) {
    const auto& target_to_sources = entry.second;
    auto sources_it = target_to_sources.find(target_id);
    if (sources_it == target_to_sources.end())
      return;

    base::ranges::for_each(sources_it->second, [&](AXNodeID source_id) {
      AXNode* source_node = tree->GetFromId(source_id);

      if (!source_node || source_nodes.count(source_node) > 0)
        return;

      source_nodes.insert(source_node);

      // GCC < 6.4 requires this pointer when calling a member
      // function in anonymous function
      this->AddEvent(source_node, Event::RELATED_NODE_CHANGED);
    });
  };

  base::ranges::for_each(tree->int_reverse_relations(), callback);
  base::ranges::for_each(tree->intlist_reverse_relations(), [&](auto& entry) {
    // Explicitly exclude relationships for which an additional event on the
    // source node would cause extra noise. For example, kRadioGroupIds
    // forms relations among all radio buttons and serves little value for
    // AT to get events on the previous radio button in the group.
    if (entry.first != ax::mojom::IntListAttribute::kRadioGroupIds)
      callback(entry);
  });
}

void AXEventGenerator::TrimEventsDueToAncestorIgnoredChanged(
    AXNode* node,
    std::map<AXNode*, IgnoredChangedStatesBitset>&
        ancestor_ignored_changed_map) {
  DCHECK(node);

  // Recursively compute and cache ancestor ignored changed results in
  // |ancestor_ignored_changed_map|, if |node|'s ancestors have become ignored
  // and the ancestor's ignored changed results have not been cached.
  if (node->parent() &&
      !base::Contains(ancestor_ignored_changed_map, node->parent())) {
    TrimEventsDueToAncestorIgnoredChanged(node->parent(),
                                          ancestor_ignored_changed_map);
  }

  // If an ancestor of |node| changed to ignored state (hide), append hide state
  // to the corresponding entry in the map for |node|. Similarly, if an ancestor
  // of |node| removed its ignored state (show), we append show state to the
  // corresponding entry in map for |node| as well. If |node| flipped its
  // ignored state as well, we want to remove various events related to
  // IGNORED_CHANGED event.
  const auto& parent_map_iter =
      ancestor_ignored_changed_map.find(node->parent());
  const auto& curr_events_iter = tree_events_.find(node->id());

  // Initialize |ancestor_ignored_changed_map[node]| with an empty bitset,
  // representing neither |node| nor its ancestor has IGNORED_CHANGED.
  IgnoredChangedStatesBitset& ancestor_ignored_changed_states =
      ancestor_ignored_changed_map[node];

  // If |ancestor_ignored_changed_map| contains an entry for |node|'s
  // ancestor's and the ancestor has either show/hide state, we want to populate
  // |node|'s show/hide state in the map based on its cached ancestor result.
  // An empty entry in |ancestor_ignored_changed_map| for |node| means that
  // neither |node| nor its ancestor has IGNORED_CHANGED.
  if (parent_map_iter != ancestor_ignored_changed_map.end()) {
    // Propagate ancestor's show/hide states to |node|'s entry in the map.
    if (HasIgnoredChangedState(parent_map_iter->second,
                               IgnoredChangedState::kHide)) {
      AddIgnoredChangedState(IgnoredChangedState::kHide,
                             &ancestor_ignored_changed_states);
    }
    if (HasIgnoredChangedState(parent_map_iter->second,
                               IgnoredChangedState::kShow)) {
      AddIgnoredChangedState(IgnoredChangedState::kShow,
                             &ancestor_ignored_changed_states);
    }

    // If |node| has IGNORED changed with show/hide state that matches one of
    // its ancestors' IGNORED changed show/hide states, we want to remove
    // |node|'s IGNORED_CHANGED related events.
    if (curr_events_iter != tree_events_.end() &&
        HasEvent(curr_events_iter->second, Event::IGNORED_CHANGED)) {
      if ((HasIgnoredChangedState(parent_map_iter->second,
                                  IgnoredChangedState::kHide) &&
           node->IsIgnored()) ||
          (HasIgnoredChangedState(parent_map_iter->second,
                                  IgnoredChangedState::kShow) &&
           !node->IsIgnored())) {
        RemoveEvent(&(curr_events_iter->second), Event::IGNORED_CHANGED);
        RemoveEventsDueToIgnoredChanged(&(curr_events_iter->second));
      }

      if (node->IsIgnored()) {
        AddIgnoredChangedState(IgnoredChangedState::kHide,
                               &ancestor_ignored_changed_states);
      } else {
        AddIgnoredChangedState(IgnoredChangedState::kShow,
                               &ancestor_ignored_changed_states);
      }
    }

    return;
  }

  // If ignored changed results for ancestors are not cached, calculate the
  // corresponding entry for |node| in the map using the ignored states and
  // events of |node|.
  if (curr_events_iter != tree_events_.end() &&
      HasEvent(curr_events_iter->second, Event::IGNORED_CHANGED)) {
    if (node->IsIgnored()) {
      AddIgnoredChangedState(IgnoredChangedState::kHide,
                             &ancestor_ignored_changed_states);
    } else {
      AddIgnoredChangedState(IgnoredChangedState::kShow,
                             &ancestor_ignored_changed_states);
    }

    return;
  }
}

void AXEventGenerator::PostprocessEvents() {
  std::map<AXNode*, IgnoredChangedStatesBitset> ancestor_ignored_changed_map;
  std::set<AXNode*> removed_subtree_created_nodes;
  std::set<AXNode*> removed_parent_changed_nodes;

  // First pass through |tree_events_|, remove events that we do not need.
  for (auto& iter : tree_events_) {
    AXNodeID node_id = iter.first;
    AXNode* node = tree_->GetFromId(node_id);

    // TODO(http://crbug.com/2279799): remove all of the cases that could
    // add a null node to |tree_events|.
    DCHECK(node);
    if (!node)
      continue;

    std::set<EventParams>& node_events = iter.second;

    // A newly created live region or alert should not *also* fire a
    // live region changed event.
    if (HasEvent(node_events, Event::ALERT) ||
        HasEvent(node_events, Event::LIVE_REGION_CREATED)) {
      RemoveEvent(&node_events, Event::LIVE_REGION_CHANGED);
    }

    if (HasEvent(node_events, Event::IGNORED_CHANGED)) {
      // If a node toggled its ignored state, we only want to fire
      // IGNORED_CHANGED event on the top most ancestor where this ignored state
      // change takes place and suppress all the descendants's IGNORED_CHANGED
      // events.
      TrimEventsDueToAncestorIgnoredChanged(node, ancestor_ignored_changed_map);
      RemoveEventsDueToIgnoredChanged(&node_events);
    }

    AXNode* parent = node->GetUnignoredParent();

    // Don't fire text attribute changed on this node if its immediate parent
    // also has text attribute changed.
    if (parent && HasEvent(node_events, Event::TEXT_ATTRIBUTE_CHANGED) &&
        tree_events_.find(parent->id()) != tree_events_.end() &&
        HasEvent(tree_events_[parent->id()], Event::TEXT_ATTRIBUTE_CHANGED)) {
      RemoveEvent(&node_events, Event::TEXT_ATTRIBUTE_CHANGED);
    }

    // Don't fire parent changed on this node if any of its ancestors also has
    // parent changed. However, if the ancestor also has subtree created, it is
    // possible that the created subtree is actually a newly unignored parent
    // of an existing node. In that instance, we need to inform ATs that the
    // existing node's parent has changed on the platform.
    if (HasEvent(node_events, Event::PARENT_CHANGED)) {
      while (parent && (tree_events_.find(parent->id()) != tree_events_.end() ||
                        base::Contains(removed_parent_changed_nodes, parent))) {
        if ((base::Contains(removed_parent_changed_nodes, parent) ||
             HasEvent(tree_events_[parent->id()], Event::PARENT_CHANGED)) &&
            !HasEvent(tree_events_[parent->id()], Event::SUBTREE_CREATED)) {
          RemoveEvent(&node_events, Event::PARENT_CHANGED);
          removed_parent_changed_nodes.insert(node);
          break;
        }
        parent = parent->GetUnignoredParent();
      }
    }

    // Don't fire parent changed on ignored events, because these nodes do not
    // exist for platform accessibility. If the node toggles the ignored state,
    // that's an IGNORED_CHANGED event and it's treated differently. In some
    // occasions it may result in a PARENT_CHANGED event on a different node
    // (see AXEventGenerator::OnIgnoredChanged).
    if (node->IsIgnored())
      RemoveEvent(&node_events, Event::PARENT_CHANGED);

    // Don't fire subtree created on this node if any of its ancestors also has
    // subtree created.
    parent = node->GetUnignoredParent();
    if (HasEvent(node_events, Event::SUBTREE_CREATED)) {
      while (parent &&
             (tree_events_.find(parent->id()) != tree_events_.end() ||
              base::Contains(removed_subtree_created_nodes, parent))) {
        if (base::Contains(removed_subtree_created_nodes, parent) ||
            HasEvent(tree_events_[parent->id()], Event::SUBTREE_CREATED)) {
          RemoveEvent(&node_events, Event::SUBTREE_CREATED);
          removed_subtree_created_nodes.insert(node);
          break;
        }
        parent = parent->GetUnignoredParent();
      }
    }
  }

  // Second pass through |tree_events_|, remove nodes that do not have any
  // events left.
  auto iter = tree_events_.begin();
  while (iter != tree_events_.end()) {
    std::set<EventParams>& node_events = iter->second;
    if (node_events.empty())
      iter = tree_events_.erase(iter);
    else
      ++iter;
  }
}

// static
void AXEventGenerator::GetRestrictionStates(ax::mojom::Restriction restriction,
                                            bool* is_enabled,
                                            bool* is_readonly) {
  switch (restriction) {
    case ax::mojom::Restriction::kDisabled:
      *is_enabled = false;
      *is_readonly = true;
      break;
    case ax::mojom::Restriction::kReadOnly:
      *is_enabled = true;
      *is_readonly = true;
      break;
    case ax::mojom::Restriction::kNone:
      *is_enabled = true;
      *is_readonly = false;
      break;
  }
}

// static
std::vector<int32_t> AXEventGenerator::ComputeIntListDifference(
    const std::vector<int32_t>& lhs,
    const std::vector<int32_t>& rhs) {
  std::set<int32_t> sorted_lhs(lhs.cbegin(), lhs.cend());
  std::set<int32_t> sorted_rhs(rhs.cbegin(), rhs.cend());

  std::vector<int32_t> result;
  std::set_symmetric_difference(sorted_lhs.cbegin(), sorted_lhs.cend(),
                                sorted_rhs.cbegin(), sorted_rhs.cend(),
                                std::back_inserter(result));
  return result;
}

std::ostream& operator<<(std::ostream& os, AXEventGenerator::Event event) {
  return os << ToString(event);
}

const char* ToString(AXEventGenerator::Event event) {
  switch (event) {
    case AXEventGenerator::Event::NONE:
      return "none";
    case AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      return "accessKeyChanged";
    case AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      return "activeDescendantChanged";
    case AXEventGenerator::Event::ALERT:
      return "alert";
    case AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      return "ariaCurrentChanged";
    case AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED:
      return "ariaNotificationsPosted";
    case AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
      return "atkTextObjectAttributeChanged";
    case AXEventGenerator::Event::ATOMIC_CHANGED:
      return "atomicChanged";
    case AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
      return "autoCompleteChanged";
    case AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
      return "autofillAvailabilityChanged";
    case AXEventGenerator::Event::BUSY_CHANGED:
      return "busyChanged";
    case AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
      return "caretBoundsChanged";
    case AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      return "checkedStateChanged";
    case AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
      return "checkedStateDescriptionChanged";
    case AXEventGenerator::Event::CHILDREN_CHANGED:
      return "childrenChanged";
    case AXEventGenerator::Event::COLLAPSED:
      return "collapsed";
    case AXEventGenerator::Event::CONTROLS_CHANGED:
      return "controlsChanged";
    case AXEventGenerator::Event::DETAILS_CHANGED:
      return "detailsChanged";
    case AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      return "describedByChanged";
    case AXEventGenerator::Event::DESCRIPTION_CHANGED:
      return "descriptionChanged";
    case AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
      return "documentSelectionChanged";
    case AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
      return "documentTitleChanged";
    case AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
      return "editableTextChanged";
    case AXEventGenerator::Event::ENABLED_CHANGED:
      return "enabledChanged";
    case AXEventGenerator::Event::EXPANDED:
      return "expanded";
    case AXEventGenerator::Event::FOCUS_CHANGED:
      return "focusChanged";
    case AXEventGenerator::Event::FLOW_FROM_CHANGED:
      return "flowFromChanged";
    case AXEventGenerator::Event::FLOW_TO_CHANGED:
      return "flowToChanged";
    case AXEventGenerator::Event::HASPOPUP_CHANGED:
      return "haspopupChanged";
    case AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
      return "hierarchicalLevelChanged";
    case AXEventGenerator::Event::IGNORED_CHANGED:
      return "ignoredChanged";
    case AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
      return "imageAnnotationChanged";
    case AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      return "invalidStatusChanged";
    case AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
      return "keyShortcutsChanged";
    case AXEventGenerator::Event::LABELED_BY_CHANGED:
      return "labeledByChanged";
    case AXEventGenerator::Event::LANGUAGE_CHANGED:
      return "languageChanged";
    case AXEventGenerator::Event::LAYOUT_INVALIDATED:
      return "layoutInvalidated";
    case AXEventGenerator::Event::LIVE_REGION_CHANGED:
      return "liveRegionChanged";
    case AXEventGenerator::Event::LIVE_REGION_CREATED:
      return "liveRegionCreated";
    case AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
      return "liveRegionNodeChanged";
    case AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
      return "liveRelevantChanged";
    case AXEventGenerator::Event::LIVE_STATUS_CHANGED:
      return "liveStatusChanged";
    case AXEventGenerator::Event::MENU_ITEM_SELECTED:
      return "menuItemSelected";
    case AXEventGenerator::Event::MENU_POPUP_END:
      return "menuPopupEnd";
    case AXEventGenerator::Event::MENU_POPUP_START:
      return "menuPopupStart";
    case AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
      return "multilineStateChanged";
    case AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
      return "multiselectableStateChanged";
    case AXEventGenerator::Event::NAME_CHANGED:
      return "nameChanged";
    case AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      return "objectAttributeChanged";
    case AXEventGenerator::Event::ORIENTATION_CHANGED:
      return "orientationChanged";
    case AXEventGenerator::Event::PARENT_CHANGED:
      return "parentChanged";
    case AXEventGenerator::Event::PLACEHOLDER_CHANGED:
      return "placeholderChanged";
    case AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
      return "positionInSetChanged";
    case AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      return "rangeValueChanged";
    case AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
      return "rangeValueMaxChanged";
    case AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
      return "rangeValueMinChanged";
    case AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
      return "rangeValueStepChanged";
    case AXEventGenerator::Event::READONLY_CHANGED:
      return "readonlyChanged";
    case AXEventGenerator::Event::RELATED_NODE_CHANGED:
      return "relatedNodeChanged";
    case AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
      return "requiredStateChanged";
    case AXEventGenerator::Event::ROLE_CHANGED:
      return "roleChanged";
    case AXEventGenerator::Event::ROW_COUNT_CHANGED:
      return "rowCountChanged";
    case AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
      return "scrollHorizontalPositionChanged";
    case AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      return "scrollVerticalPositionChanged";
    case AXEventGenerator::Event::SELECTED_CHANGED:
      return "selectedChanged";
    case AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      return "selectedChildrenChanged";
    case AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
      return "selectedValueChanged";
    case AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
      return "textSelectionChanged";
    case AXEventGenerator::Event::SET_SIZE_CHANGED:
      return "setSizeChanged";
    case AXEventGenerator::Event::SORT_CHANGED:
      return "sortChanged";
    case AXEventGenerator::Event::STATE_CHANGED:
      return "stateChanged";
    case AXEventGenerator::Event::SUBTREE_CREATED:
      return "subtreeCreated";
    case AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      return "textAttributeChanged";
    case AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      return "valueInTextFieldChanged";
    case AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      return "winIaccessibleStateChanged";
  }
}

// Convert from the string representation of an Event enum
// into the enum value. The first time this is called, builds up a map.
// Relies on the existence of ToString(enum).
bool MaybeParseGeneratedEvent(const char* attribute,
                              AXEventGenerator::Event* result) {
  static base::NoDestructor<std::map<std::string, AXEventGenerator::Event>>
      attr_map;
  if (attr_map->empty()) {
    (*attr_map)[""] = AXEventGenerator::Event::NONE;
    for (int i = static_cast<int>(AXEventGenerator::Event::NONE);
         i <= static_cast<int>(AXEventGenerator::Event::MAX_VALUE); i++) {
      auto attr = static_cast<AXEventGenerator::Event>(i);
      std::string str = ToString(attr);
      if (!base::Contains(*attr_map, str))
        (*attr_map)[str] = attr;
    }
  }
  auto iter = attr_map->find(attribute);
  if (iter != attr_map->end()) {
    *result = iter->second;
    return true;
  }

  return false;
}

AXEventGenerator::Event ParseGeneratedEvent(const char* attribute) {
  AXEventGenerator::Event event;
  if (MaybeParseGeneratedEvent(attribute, &event))
    return event;

  LOG(ERROR) << "Could not parse: " << attribute;
  NOTREACHED_IN_MIGRATION();
  return AXEventGenerator::Event::NONE;
}

}  // namespace ui
