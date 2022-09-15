// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_

#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"

// TODO(crbug.com/1357889): Remove this after migrating test logic to
// ui/accessibility.
namespace extensions {
class AutomationInternalCustomBindingsTest;
}

namespace ui {

// Virtual class that owns one or more AutomationAXTreeWrappers.
// TODO(crbug.com/1357889): Merge some of this interface with
// AXTreeManager if possible.
class AX_EXPORT AutomationTreeManagerOwner {
 public:
  AutomationTreeManagerOwner();
  virtual ~AutomationTreeManagerOwner();

  //
  // Virtual methods for sending data to the hosting bindings system.
  // TODO(crbug.com/1357889): Create implementations of these through
  // creating V8 values, and a virtual method to take V8 values and
  // dispatch them. V8 logic should go in a separate class owned by
  // this one.
  //

  // Sends an event to automation in V8 that the nodes with IDs |ids|
  // have been removed from the |tree|.
  virtual void SendNodesRemovedEvent(AXTree* tree,
                                     const std::vector<int>& ids) = 0;

  // Sends an event to automation in V8 that the |node| in |tree| has
  // undergone a |change_type| mutation.
  virtual bool SendTreeChangeEvent(ax::mojom::Mutation change_type,
                                   AXTree* tree,
                                   AXNode* node) = 0;

  // Sends an AXEvent to automation in V8.
  virtual void SendAutomationEvent(
      AXTreeID tree_id,
      const gfx::Point& mouse_location,
      const AXEvent& event,
      absl::optional<AXEventGenerator::Event> generated_event_type =
          absl::optional<AXEventGenerator::Event>()) = 0;

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

  absl::optional<gfx::Rect> GetAccessibilityFocusedLocation() const;

  void SendAccessibilityFocusedLocationChange(const gfx::Point& mouse_location);

 protected:
  friend class extensions::AutomationInternalCustomBindingsTest;

  // Given an initial AutomationAXTreeWrapper, return the
  // AutomationAXTreeWrapper and node of the focused node within this tree
  // or a focused descendant tree.
  bool GetFocusInternal(AutomationAXTreeWrapper* top_tree,
                        AutomationAXTreeWrapper** out_tree,
                        AXNode** out_node);

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
  // Gets the root(s) of a node's child tree. Multiple roots can occur when the
  // child tree uses ax::mojom::StringAttribute::kAppId.
  std::vector<AXNode*> GetRootsOfChildTree(AXNode* node) const;

  AXNode* GetNextInTreeOrder(
      AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  AXNode* GetPreviousInTreeOrder(
      AXNode* start,
      AutomationAXTreeWrapper** in_out_tree_wrapper) const;

  std::vector<int> CalculateSentenceBoundary(
      AutomationAXTreeWrapper* tree_wrapper,
      AXNode* node,
      bool start_boundary);

  void CacheAutomationTreeWrapperForTreeID(
      const AXTreeID& tree_id,
      AutomationAXTreeWrapper* tree_wrapper);

  void RemoveAutomationTreeWrapperFromCache(const AXTreeID& tree_id);

  void ClearCachedAutomationTreeWrappers();

  const AXTreeID& focus_tree_id() const { return focus_tree_id_; }

  int32_t focus_id() const { return focus_id_; }

  void SetDesktopTreeId(AXTreeID tree_id) { desktop_tree_id_ = tree_id; }

  const AXTreeID& desktop_tree_id() { return desktop_tree_id_; }

  void SetAccessibilityFocusedTreeID(AXTreeID tree_id) {
    accessibility_focused_tree_id_ = tree_id;
  }

  const AXTreeID& accessibility_focused_tree_id() const {
    return accessibility_focused_tree_id_;
  }

 private:
  std::map<AXTreeID, std::unique_ptr<AutomationAXTreeWrapper>>
      tree_id_to_tree_wrapper_map_;

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
