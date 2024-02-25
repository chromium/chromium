// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_AURALINUX_H_

#include <atspi/atspi.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer.h"

namespace ui {

using AXTreeIndexerAuraLinux = AXTreeIndexer<AtspiAccessible*,
                                             GetDOMId,
                                             std::vector<AtspiAccessible*>,
                                             ChildrenOf>;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_AURALINUX_H_
