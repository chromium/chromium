// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_tree_update_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXTreeUpdateDataView, ui::AXTreeUpdate>::Read(
    ax::mojom::AXTreeUpdateDataView data,
    ui::AXTreeUpdate* out) {
  out->has_tree_data = data.has_tree_data();
  if (!data.ReadTreeData(&out->tree_data))
    return false;
  ax::mojom::AXNodeIDPtr node_id_to_clear_ptr;
  if (!data.ReadNodeIdToClear(&node_id_to_clear_ptr)) {
    return false;
  }
  out->node_id_to_clear = node_id_to_clear_ptr->value;

  ax::mojom::AXNodeIDPtr root_id_ptr;
  if (!data.ReadRootId(&root_id_ptr)) {
    return false;
  }
  out->root_id = root_id_ptr->value;
  if (!data.ReadNodes(&out->nodes))
    return false;
  out->event_from = data.event_from();
  out->event_from_action = data.event_from_action();
  if (!data.ReadEventIntents(&out->event_intents)) {
    return false;
  }
  return data.ReadTreeChecks(&out->tree_checks);
}

}  // namespace mojo
