// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/fuchsia_types.h"

namespace ui {

AXNodeDescriptorFuchsia::AXNodeDescriptorFuchsia() = default;

AXNodeDescriptorFuchsia::AXNodeDescriptorFuchsia(AXTreeID tree_id,
                                                 AXNodeID node_id)
    : tree_id(tree_id), node_id(node_id) {}

AXNodeDescriptorFuchsia::~AXNodeDescriptorFuchsia() = default;

AXNodeUpdateFuchsia::AXNodeUpdateFuchsia() = default;
AXNodeUpdateFuchsia::AXNodeUpdateFuchsia(AXNodeUpdateFuchsia&&) = default;
AXNodeUpdateFuchsia::~AXNodeUpdateFuchsia() = default;

}  // namespace ui
