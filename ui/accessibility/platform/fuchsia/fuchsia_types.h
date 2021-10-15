// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

static constexpr int32_t kInvalidNodeId = -1;

struct NodeID {
  AXTreeID tree_id = ui::AXTreeIDUnknown();
  int32_t node_id = kInvalidNodeId;

  NodeID();
  ~NodeID();
};

struct NodeUpdate {
  fuchsia::accessibility::semantics::Node node_data;
  std::vector<NodeID> child_ids;
  absl::optional<NodeID> offset_container_id;
  bool is_root = false;

  NodeUpdate();
  ~NodeUpdate();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_
