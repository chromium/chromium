// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_update.h"

#include "ui/accessibility/ax_tree_data.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enum_util.h"

namespace ui {

AXTreeUpdate::AXTreeUpdate() = default;

AXTreeUpdate::AXTreeUpdate(const ui::AXTreeUpdate& other) = default;

AXTreeUpdate::~AXTreeUpdate() = default;

std::string AXTreeUpdate::ToString(bool verbose) const {
  std::string result;

  if (has_tree_data) {
    result += "AXTreeUpdate tree data:" + tree_data.ToString() + "\n";
  }

  if (node_id_to_clear != kInvalidAXNodeID) {
    result += "AXTreeUpdate: clear node " +
              base::NumberToString(node_id_to_clear) + "\n";
  }

  if (root_id != kInvalidAXNodeID) {
    result += "AXTreeUpdate: root id " + base::NumberToString(root_id) + "\n";
  }

  if (event_from != ax::mojom::EventFrom::kNone)
    result += "event_from=" + std::string(ui::ToString(event_from)) + "\n";
  if (event_from_action != ax::mojom::Action::kNone)
    result +=
        "event_from_action=" + std::string(ui::ToString(event_from_action)) +
        "\n";

  if (!event_intents.empty()) {
    result += "event_intents=[\n";
    for (const auto& event_intent : event_intents)
      result += "  " + event_intent.ToString() + "\n";
    result += "]\n";
  }

  // The challenge here is that we want to indent the nodes being updated
  // so that parent/child relationships are clear, but we don't have access
  // to the rest of the tree for context, so we have to try to show the
  // relative indentation of child nodes in this update relative to their
  // parents.
  std::map<AXNodeID, int> id_to_indentation;
  for (const AXNodeData& node_data : nodes) {
    int indent = id_to_indentation[node_data.id];
    result += std::string(2 * indent, ' ');
    result += node_data.ToString(/*verbose*/ verbose) + "\n";
    for (AXNodeID child_id : node_data.child_ids)
      id_to_indentation[child_id] = indent + 1;
  }

  return result;
}

}  // namespace ui
