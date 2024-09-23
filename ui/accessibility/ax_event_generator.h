// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_
#define UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_

#include <bitset>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

// Subclass of AXTreeObserver that automatically generates AXEvents to fire
// based on changes to an accessibility tree.  Every platform
// tends to want different events, so this class lets each platform
// handle the events it wants and ignore the others.
class AX_EXPORT AXEventGenerator : public AXTreeObserver {
 public:
  enum class Event : int32_t {
    NONE,
    ACCESS_KEY_CHANGED,
    ACTIVE_DESCENDANT_CHANGED,
    ALERT,
    ARIA_CURRENT_CHANGED,
    ARIA_NOTIFICATIONS_POSTED,

    // ATK treats alignment, indentation, and other format-related attributes as
    // text attributes even when they are only applicable to the entire object.
    // And it lacks an event for use when object attributes have changed.
    ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED,
    ATOMIC_CHANGED,
    AUTO_COMPLETE_CHANGED,
    AUTOFILL_AVAILABILITY_CHANGED,
    BUSY_CHANGED,
    CARET_BOUNDS_CHANGED,
    CHECKED_STATE_CHANGED,
    CHECKED_STATE_DESCRIPTION_CHANGED,
    CHILDREN_CHANGED,
    COLLAPSED,
    CONTROLS_CHANGED,
    DETAILS_CHANGED,
    DESCRIBED_BY_CHANGED,
    DESCRIPTION_CHANGED,
    DOCUMENT_SELECTION_CHANGED,
    DOCUMENT_TITLE_CHANGED,

    // TODO(nektar): Deprecate this event and replace it with
    // "VALUE_IN_TEXT_FIELD_CHANGED".
    EDITABLE_TEXT_CHANGED,
    ENABLED_CHANGED,
    EXPANDED,
    FOCUS_CHANGED,
    FLOW_FROM_CHANGED,
    FLOW_TO_CHANGED,
    HASPOPUP_CHANGED,
    HIERARCHICAL_LEVEL_CHANGED,
    IGNORED_CHANGED,
    IMAGE_ANNOTATION_CHANGED,
    INVALID_STATUS_CHANGED,
    KEY_SHORTCUTS_CHANGED,
    LABELED_BY_CHANGED,
    LANGUAGE_CHANGED,
    LAYOUT_INVALIDATED,  // Fired when aria-busy turns from true to false.

    // Fired only on the root of the ARIA live region.
    LIVE_REGION_CHANGED,
    // Fired only on the root of the ARIA live region.
    LIVE_REGION_CREATED,
    // Fired on all the nodes within the ARIA live region excluding its root.
    LIVE_REGION_NODE_CHANGED,
    // Fired only on the root of the ARIA live region.
    LIVE_RELEVANT_CHANGED,
    // Fired only on the root of the ARIA live region.
    LIVE_STATUS_CHANGED,
    MENU_ITEM_SELECTED,
    MENU_POPUP_END,
    MENU_POPUP_START,
    MULTILINE_STATE_CHANGED,
    MULTISELECTABLE_STATE_CHANGED,
    NAME_CHANGED,
    OBJECT_ATTRIBUTE_CHANGED,
    ORIENTATION_CHANGED,
    PARENT_CHANGED,
    PLACEHOLDER_CHANGED,
    POSITION_IN_SET_CHANGED,
    RANGE_VALUE_CHANGED,
    RANGE_VALUE_MAX_CHANGED,
    RANGE_VALUE_MIN_CHANGED,
    RANGE_VALUE_STEP_CHANGED,
    READONLY_CHANGED,
    RELATED_NODE_CHANGED,
    REQUIRED_STATE_CHANGED,
    ROLE_CHANGED,
    ROW_COUNT_CHANGED,
    SCROLL_HORIZONTAL_POSITION_CHANGED,
    SCROLL_VERTICAL_POSITION_CHANGED,
    SELECTED_CHANGED,
    SELECTED_CHILDREN_CHANGED,
    SELECTED_VALUE_CHANGED,
    SET_SIZE_CHANGED,
    SORT_CHANGED,
    STATE_CHANGED,
    SUBTREE_CREATED,
    TEXT_ATTRIBUTE_CHANGED,
    TEXT_SELECTION_CHANGED,
    VALUE_IN_TEXT_FIELD_CHANGED,

    // This event is fired for the exact set of attributes that affect the
    // MSAA/IAccessible state on Windows. It is not needed on other platforms,
    // but it is very natural to compute in this class.
    WIN_IACCESSIBLE_STATE_CHANGED,
    MAX_VALUE = WIN_IACCESSIBLE_STATE_CHANGED,
  };

  // For distinguishing between show and hide state when a node has
  // an IGNORED_CHANGED event.
  enum class IgnoredChangedState : uint8_t { kShow, kHide, kCount = 2 };

  struct AX_EXPORT EventParams final {
    explicit EventParams(Event event);
    EventParams(Event event,
                ax::mojom::EventFrom event_from,
                ax::mojom::Action event_from_action,
                const std::vector<AXEventIntent>& event_intents);
    EventParams(const EventParams& other);
    ~EventParams();

    EventParams& operator=(const EventParams& other);
    bool operator==(const EventParams& rhs) const;
    bool operator<(const EventParams& rhs) const;

    Event event;
    ax::mojom::EventFrom event_from = ax::mojom::EventFrom::kNone;
    ax::mojom::Action event_from_action;
    std::vector<AXEventIntent> event_intents;
  };

  struct AX_EXPORT TargetedEvent final {
    TargetedEvent(AXNodeID node_id, const EventParams& event_params);
    ~TargetedEvent();

    const AXNodeID node_id;
    const raw_ref<const EventParams, DanglingUntriaged> event_params;
  };

  class AX_EXPORT Iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = TargetedEvent;
    using difference_type = std::ptrdiff_t;
    using pointer = TargetedEvent*;
    using reference = TargetedEvent&;

    Iterator(
        std::map<AXNodeID, std::set<EventParams>>::const_iterator
            map_start_iter,
        std::map<AXNodeID, std::set<EventParams>>::const_iterator map_end_iter);
    Iterator(const Iterator& other);
    ~Iterator();

    Iterator& operator=(const Iterator& other);
    Iterator& operator++();
    Iterator operator++(int);  // Postfix increment.
    value_type operator*() const;

   private:
    AX_EXPORT friend bool operator==(const Iterator& lhs, const Iterator& rhs);
    AX_EXPORT friend bool operator!=(const Iterator& lhs, const Iterator& rhs);
    AX_EXPORT friend void swap(Iterator& lhs, Iterator& rhs);

    std::map<AXNodeID, std::set<EventParams>>::const_iterator map_iter_;
    std::map<AXNodeID, std::set<EventParams>>::const_iterator map_end_iter_;
    std::set<EventParams>::const_iterator set_iter_;
  };

  // For storing ignored changed states for a particular node. We use bitset as
  // the underlying data structure to improve memory usage.
  // We use the index of AXEventGenerator::IgnoredChangedState enum
  // to access the bitset data.
  // e.g. AXEventGenerator::IgnoredChangedState::kShow has index 0 in the
  // IgnoredChangedState enum. If |IgnoredChangedStatesBitset[0]| is set, it
  // means IgnoredChangedState::kShow is present. Similarly, kHide has index 1
  // in the enum, and it corresponds to |IgnoredChangedStatesBitset[1]|.
  using IgnoredChangedStatesBitset =
      std::bitset<static_cast<size_t>(IgnoredChangedState::kCount)>;
  using const_iterator = Iterator;
  using iterator = Iterator;
  using value_type = TargetedEvent;

  // If you use this constructor, you must call SetTree
  // before using this class.
  AXEventGenerator();

  // Automatically registers itself as the observer of |tree| and
  // clears it on desctruction. |tree| must be valid for the lifetime
  // of this object.
  explicit AXEventGenerator(AXTree* tree);

  ~AXEventGenerator() override;

  // Clears this class as the observer of the previous tree that was
  // being monitored, if any, and starts monitoring |new_tree|, if not
  // nullptr. Note that |new_tree| must be valid for the lifetime of
  // this object or until you call SetTree again.
  void SetTree(AXTree* new_tree);

  // Nulls-out |tree_| without accessing it or destroying it.
  void ReleaseTree();

  //
  // Methods that make this class behave like an STL container, which simplifies
  // the process of iterating through generated events.
  //

  bool empty() const;
  size_t size() const;
  Iterator begin() const;
  Iterator end() const;

  // Clear any previously added events.
  void ClearEvents();

  // This is called automatically based on changes to the tree observed
  // by AXTreeObserver, but you can also call it directly to add events
  // and retrieve them later.
  //
  // Note that events are organized by node and then by event id to
  // efficiently remove duplicates, so events won't be retrieved in the
  // same order they were added.
  void AddEvent(AXNode* node, Event event);

  // Registers for events on the node or one of its descendants.
  // Registration offers a more performant path for event generation.
  // See the implementation for currently supported events for registration.
  // Gradually move as many events to registration as possible.
  void RegisterEventOnNode(Event event_type, AXNodeID node_id);
  void UnregisterEventOnNode(Event event_type, AXNodeID node_id);

  void AddEventsForTesting(const AXNode& node,
                           const std::set<EventParams>& events);

 protected:
  // AXTreeObserver overrides.
  void OnIgnoredWillChange(
      AXTree* tree,
      AXNode* node,
      bool is_ignored_new_value,
      bool is_changing_unignored_parents_children) override;
  void OnNodeDataChanged(AXTree* tree,
                         const AXNodeData& old_node_data,
                         const AXNodeData& new_node_data) override;
  void OnRoleChanged(AXTree* tree,
                     AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override;
  void OnIgnoredChanged(AXTree* tree,
                        AXNode* node,
                        bool is_ignored_new_value) override;
  void OnStateChanged(AXTree* tree,
                      AXNode* node,
                      ax::mojom::State state,
                      bool new_value) override;
  void OnStringAttributeChanged(AXTree* tree,
                                AXNode* node,
                                ax::mojom::StringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override;
  void OnIntAttributeChanged(AXTree* tree,
                             AXNode* node,
                             ax::mojom::IntAttribute attr,
                             int32_t old_value,
                             int32_t new_value) override;
  void OnFloatAttributeChanged(AXTree* tree,
                               AXNode* node,
                               ax::mojom::FloatAttribute attr,
                               float old_value,
                               float new_value) override;
  void OnBoolAttributeChanged(AXTree* tree,
                              AXNode* node,
                              ax::mojom::BoolAttribute attr,
                              bool new_value) override;
  void OnIntListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::IntListAttribute attr,
      const std::vector<int32_t>& old_value,
      const std::vector<int32_t>& new_value) override;
  void OnTreeDataChanged(AXTree* tree,
                         const AXTreeData& old_data,
                         const AXTreeData& new_data) override;
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnNodeWillBeReparented(AXTree* tree, AXNode* node) override;
  void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) override;
  void OnNodeDeleted(AXTree* tree, AXNodeID node_id) override;
  void OnNodeReparented(AXTree* tree, AXNode* node) override;
  void OnNodeCreated(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

 private:
  static void GetRestrictionStates(ax::mojom::Restriction restriction,
                                   bool* is_enabled,
                                   bool* is_readonly);

  // Returns a vector of values unique to either |lhs| or |rhs|
  static std::vector<int32_t> ComputeIntListDifference(
      const std::vector<int32_t>& lhs,
      const std::vector<int32_t>& rhs);

  // Return true if this node can fire live region events when it's removed.
  bool IsRemovalRelevantInLiveRegion(AXNode* node);

  void FireLiveRegionEvents(AXNode* node, bool is_removal);
  void FireActiveDescendantEvents();
  // If the given target node is inside a text field and the node's modification
  // could affect the field's value, generates an `VALUE_IN_TEXT_FIELD_CHANGED`
  // on the text field that contains the node.
  void FireValueInTextFieldChangedEventIfNecessary(AXTree* tree,
                                                   AXNode* target_node);
  void FireRelationSourceEvents(AXTree* tree, AXNode* target_node);

  // Remove excessive events for a tree update containing node.
  // We remove certain events on a node when it flips its IGNORED state to
  // either show/hide and one of the node's ancestor has also flipped its
  // IGNORED state in the same way (show/hide) in the tree update.
  // |ancestor_has_ignored_map| contains if a node's ancestor has changed to
  // IGNORED state.
  // Map's key is an AXNode.
  // Map's value is a std::bitset containing IgnoredChangedStates(kShow/kHide).
  // - Map's value IgnoredChangedStatesBitset contains kShow if an ancestor
  //   of node removed its IGNORED state.
  // - Map's value IgnoredChangedStatesBitset contains kHide if an ancestor
  //   of node changed to IGNORED state.
  // - When IgnoredChangedStatesBitset is not set, it means neither the
  //   node nor its ancestor has IGNORED_CHANGED.
  void TrimEventsDueToAncestorIgnoredChanged(
      AXNode* node,
      std::map<AXNode*, IgnoredChangedStatesBitset>&
          ancestor_ignored_changed_map);
  void PostprocessEvents();

  raw_ptr<AXTree> tree_ = nullptr;  // Not owned.
  std::map<AXNodeID, std::set<EventParams>> tree_events_;

  // Valid between the call to OnIntAttributeChanged and the call to
  // OnAtomicUpdateFinished. List of nodes whose active descendant changed.
  std::vector<raw_ptr<AXNode, VectorExperimental>> active_descendant_changed_;

  // Keeps track of nodes that have changed their state from ignored to
  // unignored, but which used to be in an invisible subtree. We should not fire
  // `Event::PARENT_CHANGED` on any of their children because they were
  // previously unknown to ATs.
  std::set<AXNodeID> nodes_to_suppress_parent_changed_on_;

  // Registered events for a given node.
  std::map<Event, std::set<AXNodeID>> registered_event_to_node_ids_;

  // Please make sure that this ScopedObservation is always declared last in
  // order to prevent any use-after-free.
  base::ScopedObservation<AXTree, AXTreeObserver> tree_event_observation_{this};
};

AX_EXPORT std::ostream& operator<<(std::ostream& os,
                                   AXEventGenerator::Event event);
AX_EXPORT const char* ToString(AXEventGenerator::Event event);

// Parses the attribute and updates |result| and returns true if a match is
// found, or returns false if no match is found.
AX_EXPORT bool MaybeParseGeneratedEvent(const char* attribute,
                                        AXEventGenerator::Event* result);

// Does a NOTREACHED if no match is found.
AX_EXPORT AXEventGenerator::Event ParseGeneratedEvent(const char* attribute);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_
