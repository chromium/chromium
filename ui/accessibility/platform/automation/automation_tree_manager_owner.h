// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_

#include <vector>
#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "ui/gfx/geometry/point.h"
#include "v8/include/v8-isolate.h"

namespace ui {
class AutomationV8Bindings;

// Virtual class that owns one or more AutomationAXTreeWrappers.
// TODO(crbug.com/1357889): Merge some of this interface with
// AXTreeManager if possible.
class COMPONENT_EXPORT(AX_PLATFORM) AutomationTreeManagerOwner
    : public ax::mojom::Automation {
 public:
  AutomationTreeManagerOwner();
  ~AutomationTreeManagerOwner() override;

  virtual AutomationV8Bindings* GetAutomationV8Bindings() const = 0;
  virtual void NotifyTreeEventListenersChanged() = 0;

  // Sends an event to automation in V8 that the nodes with IDs |ids|
  // have been removed from the |tree|.
  void SendNodesRemovedEvent(AXTree* tree, const std::vector<int>& ids);

  // Sends an event to automation in V8 that the |node| in |tree| has
  // undergone a |change_type| mutation.
  bool SendTreeChangeEvent(ax::mojom::Mutation change_type,
                           AXTree* tree,
                           AXNode* node);

  // Sends an AXEvent to automation in V8.
  void SendAutomationEvent(
      AXTreeID tree_id,
      const gfx::Point& mouse_location,
      const AXEvent& event,
      std::optional<AXEventGenerator::Event> generated_event_type =
          std::optional<AXEventGenerator::Event>());

  // Gets the hosting node in a parent tree.
  AXNode* GetHostInParentTree(
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  AutomationAXTreeWrapper* GetAutomationAXTreeWrapperFromTreeID(
      AXTreeID tree_id) const;

  // Given a tree (|in_out_tree_wrapper|) and a node, returns the parent.
  // If |node| is the root of its tree, the return value will be the host
  // node of the parent tree and |in_out_tree_wrapper| will be updated to
  // point to that parent tree.
  //
  // |should_use_app_id|, if true, considers
  // ax::mojom::IntAttribute::kChildTreeNodeAppId when moving to ancestors.
  // |requires_unignored|, if true, keeps moving to ancestors until an unignored
  // ancestor parent is found.
  AXNode* GetParent(AXNode* node,
                    AutomationAXTreeWrapper** in_out_tree_wrapper,
                    bool should_use_app_id = true,
                    bool requires_unignored = true) const;

  void MaybeSendFocusAndBlur(AutomationAXTreeWrapper* tree,
                             const AXTreeID& tree_id,
                             const std::vector<AXTreeUpdate>& updates,
                             const std::vector<AXEvent>& events,
                             gfx::Point mouse_location);

  std::optional<gfx::Rect> GetAccessibilityFocusedLocation() const;

  void SendAccessibilityFocusedLocationChange(const gfx::Point& mouse_location);

  // Adjust the bounding box of a node from local to global coordinates,
  // walking up the parent hierarchy to offset by frame offsets and
  // scroll offsets.
  // If |clip_bounds| is false, the bounds of the node will not be clipped
  // to the ancestors bounding boxes if needed. Regardless of clipping, results
  // are returned in global coordinates.
  gfx::Rect ComputeGlobalNodeBounds(AutomationAXTreeWrapper* tree_wrapper,
                                    AXNode* node,
                                    gfx::RectF local_bounds = gfx::RectF(),
                                    bool* offscreen = nullptr,
                                    bool clip_bounds = true) const;

  std::vector<int> CalculateSentenceBoundary(
      AutomationAXTreeWrapper* tree_wrapper,
      AXNode* node,
      bool start_boundary);

  bool GetFocus(AXTreeID* focused_tree_id, int* node_id) const;

  size_t GetChildCount(AXNode* node) const;

  // Gets the child IDs of the given node, and finds the AXTreeID that the node
  // belongs to and sets |result_tree_id|.
  std::vector<int> GetChildIDs(AXNode* node, AXTreeID* result_tree_id) const;

  // Returns false unable to get bounds for range on this node, for
  // example if it is not an inline text box.
  bool GetBoundsForRange(AutomationAXTreeWrapper* tree_wrapper,
                         AXNode* node,
                         int start,
                         int end,
                         bool clipped,
                         gfx::Rect* result) const;

  const char* GetName(AXNode* node) const;

  bool GetNextTextMatch(AutomationAXTreeWrapper* tree_wrapper,
                        AXNode* node,
                        const std::string& search_str,
                        bool backward,
                        AXTreeID* result_tree_id,
                        int* result_node_id) const;

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Sets |child_tree_id| and |child_node_id|. Returns false if not successful.
  bool GetChildIDAtIndex(const AXTreeID& ax_tree_id,
                         int node_id,
                         int index,
                         AXTreeID* child_tree_id,
                         int* child_node_id) const;

  // Sets the |node_id| and |tree_id| for the node which has global
  // accessibility focus, or returns false if it cannot find the focus.
  bool GetAccessibilityFocus(AXTreeID* tree_id, int* node_id) const;

  // Find the node with the given ID in the tree with the given ID, or
  // returns nullptr if not found.
  AXNode* GetNodeFromTree(const AXTreeID& tree_id, int node_id) const;

  void AddTreeChangeObserver(int observer_id, TreeChangeObserverFilter filter);
  void RemoveTreeChangeObserver(int observer_id);
  void TreeEventListenersChanged(AutomationAXTreeWrapper* tree_wrapper);
  bool ShouldSendTreeChangeEvent(ax::mojom::Mutation change_type,
                                 AXTree* tree,
                                 AXNode* node) const;
  void DestroyAccessibilityTree(const AXTreeID& tree_id);
  void ClearCachedAccessibilityTrees();

  // Calculate the state of the node with ID |node_id|, or returns false if the
  // node cannot be found in the tree with ID |tree_id|.
  bool CalculateNodeState(const AXTreeID& tree_id,
                          int node_id,
                          uint32_t* node_state,
                          bool* offscreen,
                          bool* focused) const;

  void SetAccessibilityFocus(AXTreeID tree_id);

  void SetDesktopTreeId(AXTreeID tree_id) { desktop_tree_id_ = tree_id; }

 protected:
  friend class AutomationTreeManagerOwnerTest;

  // Invalidates this AutomationTreeManagerOnwer.
  void Invalidate();

  bool HasTreesWithEventListeners() const;

  void MaybeSendOnAllAutomationEventListenersRemoved();

  // ax::mojom::Automation:
  void DispatchTreeDestroyedEvent(const AXTreeID& tree_id) override;
  void DispatchAccessibilityEvents(const AXTreeID& tree_id,
                                   const std::vector<AXTreeUpdate>& updates,
                                   const gfx::Point& mouse_location,
                                   const std::vector<AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const AXTreeID& tree_id,
      int32_t node_id,
      const AXRelativeBounds& bounds) override;
  void DispatchActionResult(const AXActionData& data, bool result) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void DispatchGetTextLocationResult(
      const AXActionData& data,
      const std::optional<gfx::Rect>& rect) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Mojo receiver to the Automation interface, implemented by this class.
  // Listed as a protected member so that derived classes can reset its status
  // depending on their use cases.
  mojo::AssociatedReceiver<ax::mojom::Automation> receiver_;

 private:
  // Gets the root(s) of a node's child tree. Multiple roots can occur when the
  // child tree uses ax::mojom::StringAttribute::kAppId.
  std::vector<AXNode*> GetRootsOfChildTree(AXNode* node) const;

  AXNode* GetNextInTreeOrder(
      AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  AXNode* GetPreviousInTreeOrder(
      AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  // Given an initial AutomationAXTreeWrapper, return the
  // AutomationAXTreeWrapper and node of the focused node within this tree
  // or a focused descendant tree.
  bool GetFocusInternal(AutomationAXTreeWrapper* top_tree,
                        AutomationAXTreeWrapper** out_tree,
                        AXNode** out_node) const;

  void CacheAutomationTreeWrapperForTreeID(
      const AXTreeID& tree_id,
      AutomationAXTreeWrapper* tree_wrapper);

  void RemoveAutomationTreeWrapperFromCache(const AXTreeID& tree_id);

  void ClearCachedAutomationTreeWrappers();

  void UpdateOverallTreeChangeObserverFilter();

  std::map<AXTreeID, std::unique_ptr<AutomationAXTreeWrapper>>
      tree_id_to_tree_wrapper_map_;

  // Keeps track of all trees with event listeners.
  std::set<AXTreeID> trees_with_event_listeners_;

  std::vector<TreeChangeObserver> tree_change_observers_;

  // A bit-map of api::automation::TreeChangeObserverFilter.
  int tree_change_observer_overall_filter_ = 0;

  // Keeps track  of the single desktop tree, if it exists.
  AXTreeID desktop_tree_id_ = AXTreeIDUnknown();

  // The global accessibility focused id set by a js client. Differs from focus
  // as used in AXTree.
  AXTreeID accessibility_focused_tree_id_ = AXTreeIDUnknown();

  // The global focused tree id.
  AXTreeID focus_tree_id_;

  // The global focused node id.
  int32_t focus_id_ = -1;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_
