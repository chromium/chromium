// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_

#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"

namespace ui {

// Virtual class that owns one or more AutomationAXTreeWrappers.
// TODO(crbug.com/1357889): Refactor extensions-agnostic logic from
// AutomationInternalCustomBindings here.
// TODO(crbug.com/1357889): Merge some of this interface with
// AXTreeManager if possible.
class AX_EXPORT AutomationTreeManagerOwner {
 public:
  virtual AutomationAXTreeWrapper* GetAutomationAXTreeWrapperFromTreeID(
      AXTreeID tree_id) const = 0;

  // Given a tree (|in_out_tree_wrapper|) and a node, returns the parent.
  // If |node| is the root of its tree, the return value will be the host
  // node of the parent tree and |in_out_tree_wrapper| will be updated to
  // point to that parent tree.
  //
  // |should_use_app_id|, if true, considers
  // ax::mojom::IntAttribute::kChildTreeNodeAppId when moving to ancestors.
  // |requires_unignored|, if true, keeps moving to ancestors until an unignored
  // ancestor parent is found.
  virtual ui::AXNode* GetParent(
      ui::AXNode* node,
      ui::AutomationAXTreeWrapper** in_out_tree_wrapper,
      bool should_use_app_id,
      bool requires_unignored) const = 0;

  // Gets the hosting node in a parent tree.
  virtual AXNode* GetHostInParentTree(
      AutomationAXTreeWrapper** in_out_tree_wrapper) const = 0;

  virtual void SendNodesRemovedEvent(AXTree* tree,
                                     const std::vector<int>& ids) = 0;

  virtual bool SendTreeChangeEvent(ax::mojom::Mutation change_type,
                                   AXTree* tree,
                                   AXNode* node) = 0;

  // TODO(crubg.com/1357889): Use AXEventWrapper. instead of |event| and
  // |generated_event_type|.
  virtual void SendAutomationEvent(
      AXTreeID tree_id,
      const gfx::Point& mouse_location,
      const AXEvent& event,
      absl::optional<AXEventGenerator::Event> generated_event_type =
          absl::optional<AXEventGenerator::Event>()) = 0;

  virtual void MaybeSendFocusAndBlur(AutomationAXTreeWrapper* tree,
                                     const AXTreeID& tree_id,
                                     const std::vector<AXTreeUpdate>& updates,
                                     const std::vector<AXEvent>& events,
                                     gfx::Point mouse_location) = 0;

  virtual absl::optional<gfx::Rect> GetAccessibilityFocusedLocation() const = 0;

  virtual void SendAccessibilityFocusedLocationChange(
      const gfx::Point& mouse_location) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_TREE_MANAGER_OWNER_H_
