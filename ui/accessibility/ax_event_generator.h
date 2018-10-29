// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_
#define UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_

#include <map>
#include <set>
#include <vector>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

// Subclass of AXTreeDelegate that automatically generates AXEvents to fire
// based on changes to an accessibility tree.  Every platform
// tends to want different events, so this class lets each platform
// handle the events it wants and ignore the others.
class AX_EXPORT AXEventGenerator : public AXTreeDelegate {
 public:
  enum class Event : int32_t {
    ACTIVE_DESCENDANT_CHANGED,
    ALERT,
    CHECKED_STATE_CHANGED,
    CHILDREN_CHANGED,
    COLLAPSED,
    DESCRIPTION_CHANGED,
    DOCUMENT_SELECTION_CHANGED,
    DOCUMENT_TITLE_CHANGED,
    EXPANDED,
    INVALID_STATUS_CHANGED,
    LIVE_REGION_CHANGED,  // Fired on the root of a live region.
    LIVE_REGION_CREATED,
    LIVE_REGION_NODE_CHANGED,  // Fired on a node within a live region.
    LOAD_COMPLETE,
    LOAD_START,
    MENU_ITEM_SELECTED,
    NAME_CHANGED,
    OTHER_ATTRIBUTE_CHANGED,
    RELATED_NODE_CHANGED,
    ROLE_CHANGED,
    ROW_COUNT_CHANGED,
    SCROLL_POSITION_CHANGED,
    SELECTED_CHANGED,
    SELECTED_CHILDREN_CHANGED,
    STATE_CHANGED,
    VALUE_CHANGED,
  };

  struct EventParams {
    EventParams(Event event, ax::mojom::EventFrom event_from);
    Event event;
    ax::mojom::EventFrom event_from;

    bool operator==(const EventParams& rhs);
    bool operator<(const EventParams& rhs) const;
  };

  struct TargetedEvent {
    TargetedEvent(ui::AXNode* node, const EventParams& event_params);
    ui::AXNode* node;
    const EventParams& event_params;
  };

  class AX_EXPORT Iterator
      : public std::iterator<std::input_iterator_tag, TargetedEvent> {
   public:
    Iterator(
        const std::map<AXNode*, std::set<EventParams>>& map,
        const std::map<AXNode*, std::set<EventParams>>::const_iterator& head);
    Iterator(const Iterator& other);
    ~Iterator();

    bool operator!=(const Iterator& rhs) const;
    Iterator& operator++();
    TargetedEvent operator*() const;

   private:
    const std::map<AXNode*, std::set<EventParams>>& map_;
    std::map<AXNode*, std::set<EventParams>>::const_iterator map_iter_;
    std::set<EventParams>::const_iterator set_iter_;
  };

  // If you use this constructor, you must call SetTree
  // before using this class.
  AXEventGenerator();

  // Automatically registers itself as the delegate of |tree| and
  // clears it on desctruction. |tree| must be valid for the lifetime
  // of this object.
  explicit AXEventGenerator(AXTree* tree);

  ~AXEventGenerator() override;

  // Clears this class as the delegate of the previous tree that was
  // being monitored, if any, and starts monitoring |new_tree|, if not
  // nullptr. Note that |new_tree| must be valid for the lifetime of
  // this object or until you call SetTree again.
  void SetTree(AXTree* new_tree);

  // Null |tree_| without accessing it or destroying it.
  void ReleaseTree();

  Iterator begin() const {
    return Iterator(tree_events_, tree_events_.begin());
  }
  Iterator end() const { return Iterator(tree_events_, tree_events_.end()); }

  // Clear any previously added events.
  void ClearEvents();

  // This is called automatically based on changes to the tree observed
  // by AXTreeDelegate, but you can also call it directly to add events
  // and retrieve them later.
  //
  // Note that events are organized by node and then by event id to
  // efficiently remove duplicates, so events won't be retrieved in the
  // same order they were added.
  void AddEvent(ui::AXNode* node, Event event);

  // Set the event_from field for all subsequent generated events.
  void set_event_from(ax::mojom::EventFrom event_from) {
    event_from_ = event_from;
  }

 protected:
  // AXTreeDelegate overrides.
  void OnNodeDataWillChange(AXTree* tree,
                            const AXNodeData& old_node_data,
                            const AXNodeData& new_node_data) override;
  void OnRoleChanged(AXTree* tree,
                     AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override;
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
  void OnStringListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::StringListAttribute attr,
      const std::vector<std::string>& old_value,
      const std::vector<std::string>& new_value) override;
  void OnTreeDataChanged(AXTree* tree,
                         const ui::AXTreeData& old_data,
                         const ui::AXTreeData& new_data) override;
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnNodeWillBeReparented(AXTree* tree, AXNode* node) override;
  void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) override;
  void OnNodeCreated(AXTree* tree, AXNode* node) override;
  void OnNodeReparented(AXTree* tree, AXNode* node) override;
  void OnNodeChanged(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;

 private:
  void FireLiveRegionEvents(AXNode* node);
  void FireActiveDescendantEvents();
  void FireRelationSourceEvents(AXTree* tree, AXNode* target_node);
  bool ShouldFireLoadEvents(AXNode* node);

  AXTree* tree_ = nullptr;  // Not owned.
  std::map<AXNode*, std::set<EventParams>> tree_events_;

  // Valid between the call to OnIntAttributeChanged and the call to
  // OnAtomicUpdateFinished. List of nodes whose active descendant changed.
  std::vector<AXNode*> active_descendant_changed_;

  // The value of the event from field in TargetedEvent.
  ax::mojom::EventFrom event_from_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_EVENT_GENERATOR_H_
