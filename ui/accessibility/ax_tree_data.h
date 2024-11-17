// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_DATA_H_
#define UI_ACCESSIBILITY_AX_TREE_DATA_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// The data associated with an accessibility tree that's global to the
// tree and not associated with any particular node in the tree.
struct AX_BASE_EXPORT AXTreeData {
  AXTreeData();
  AXTreeData(const AXTreeData& other);
  virtual ~AXTreeData();

  // Return a string representation of this data, for debugging.
  virtual std::string ToString() const;

  // This is a simple serializable struct. All member variables should be
  // public and copyable.

  // The globally unique ID of this accessibility tree.
  AXTreeID tree_id;

  // The ID of the accessibility tree that this tree is contained in, if any.
  AXTreeID parent_tree_id;

  // The ID of the accessibility tree that has focus. This is typically set
  // on the root frame in a frame tree.
  AXTreeID focused_tree_id;

  // Attributes specific to trees that are web frames.
  std::string doctype;
  bool loaded = false;
  float loading_progress = 0.0f;
  std::string mimetype;
  std::string title;
  std::string url;

  // The node with keyboard focus within this tree, if any, or
  // kInvalidAXNodeID if no node in this tree has focus.
  AXNodeID focus_id = kInvalidAXNodeID;

  // The current text selection within this tree, if any, expressed as the
  // node ID and character offset of the anchor (selection start) and focus
  // (selection end). If the offset could correspond to a position on two
  // different lines, sel_upstream_affinity means the cursor is on the first
  // line, otherwise it's on the second line.
  // Most use cases will want to use OwnerTree::GetUnignoredSelection.
  bool sel_is_backward = false;
  AXNodeID sel_anchor_object_id = kInvalidAXNodeID;
  int32_t sel_anchor_offset = -1;
  ax::mojom::TextAffinity sel_anchor_affinity;
  AXNodeID sel_focus_object_id = kInvalidAXNodeID;
  int32_t sel_focus_offset = -1;
  ax::mojom::TextAffinity sel_focus_affinity;

  // The node that's used as the root scroller. On some platforms
  // like Android we need to ignore accessibility scroll offsets for
  // that node and get them from the viewport instead.
  AXNodeID root_scroller_id = kInvalidAXNodeID;

  // Metadata from an HTML HEAD, such as <meta> tags. Stored here
  // unparsed because the only applications that need these just want
  // raw strings. Only included if the kHTMLMetadata AXMode is enabled.
  std::vector<std::string> metadata;
};

AX_BASE_EXPORT bool operator==(const AXTreeData& lhs, const AXTreeData& rhs);
AX_BASE_EXPORT bool operator!=(const AXTreeData& lhs, const AXTreeData& rhs);

AX_BASE_EXPORT const AXTreeData& AXTreeDataUnknown();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_DATA_H_
