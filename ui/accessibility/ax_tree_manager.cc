// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

AXTreeManager::AXTreeManager()
    : ax_tree_id_(AXTreeIDUnknown()), ax_tree_(nullptr) {}

AXTreeManager::AXTreeManager(const AXTreeID& tree_id,
                             std::unique_ptr<AXTree> tree)
    : ax_tree_id_(tree_id), ax_tree_(std::move(tree)) {}

AXTreeManager::~AXTreeManager() = default;

}  // namespace ui
