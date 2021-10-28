// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

struct AXNodeDescriptorFuchsia {
  AXTreeID tree_id = ui::AXTreeIDUnknown();
  AXNodeID node_id = ui::kInvalidAXNodeID;

  AXNodeDescriptorFuchsia();
  ~AXNodeDescriptorFuchsia();
};

struct AXNodeUpdateFuchsia {
  fuchsia::accessibility::semantics::Node node_data;
  std::vector<AXNodeDescriptorFuchsia> child_ids;
  absl::optional<AXNodeDescriptorFuchsia> offset_container_id;
  bool is_root = false;

  AXNodeUpdateFuchsia();
  ~AXNodeUpdateFuchsia();
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_FUCHSIA_TYPES_H_
