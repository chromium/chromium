// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ACTION_DATA_H_
#define UI_ACCESSIBILITY_AX_ACTION_DATA_H_

#include <string>
#include <utility>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// A compact representation of an accessibility action and the arguments
// associated with that action.
struct AX_BASE_EXPORT AXActionData {
  AXActionData();
  AXActionData(const AXActionData& other);
  ~AXActionData();

  // This is a simple serializable struct. All member variables should be
  // public and copyable.

  // See the ax::mojom::Action enums in ax_enums.mojom for explanations of which
  // parameters apply.

  // The action to take.
  ax::mojom::Action action;

  // The ID of the tree that this action should be performed on.
  AXTreeID target_tree_id = AXTreeIDUnknown();

  // The source extension id (if any) of this action.
  std::string source_extension_id;

  // The ID of the node that this action should be performed on.
  //
  // Note that `target_role` should not be set if `target_node_id` is set, or
  // vice versa.
  AXNodeID target_node_id = kInvalidAXNodeID;

  // Searches for the first node with the given role and performs the action on
  // that node.
  //
  // Note that `target_role` should not be set if `target_node_id` is set, or
  // vice versa.
  ax::mojom::Role target_role = ax::mojom::Role::kUnknown;

  // The request id of this action tracked by the client.
  int request_id = -1;

  // Use enums from ax::mojom::ActionFlags
  int flags = 0;

  // For an action that creates a selection, the selection anchor and focus
  // (see ax_tree_data.h for definitions).
  int anchor_node_id = -1;
  int anchor_offset = -1;

  int focus_node_id = -1;
  int focus_offset = -1;

  // Start index of the text which should be queried for.
  int32_t start_index = -1;

  // End index of the text which should be queried for.
  int32_t end_index = -1;

  // For custom action.
  AXNodeID custom_action_id = kInvalidAXNodeID;

  // The target rect for the action.
  gfx::Rect target_rect;

  // The target point for the action in screen coordinates.
  gfx::Point target_point;

  // The new value for a node, for the SET_VALUE action. UTF-8 encoded.
  std::string value;

  // The row and column to move to. Used with the
  // ax::mojom::Action::kScrollToPositionAtRowColumn action.
  //
  // Supported by Android.
  std::pair<int, int> row_column;

  // The event to fire in response to a HIT_TEST action.
  ax::mojom::Event hit_test_event_to_fire;

  // The scroll alignment to use for a SCROLL_TO_MAKE_VISIBLE action. The
  // scroll alignment controls where a node is scrolled within the viewport.
  ax::mojom::ScrollAlignment horizontal_scroll_alignment;
  ax::mojom::ScrollAlignment vertical_scroll_alignment;

  // The behavior to use for a SCROLL_TO_MAKE_VISIBLE. This controls whether or
  // not the viewport is scrolled when the node is already visible.
  ax::mojom::ScrollBehavior scroll_behavior;

  // The child tree that needs to be stitched at `target_node_id`. Only used by
  // `ax::mojom::Action::kStitchChildTree`.
  AXTreeID child_tree_id = AXTreeIDUnknown();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ACTION_DATA_H_
