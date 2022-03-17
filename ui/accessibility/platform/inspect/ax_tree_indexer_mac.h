// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_

#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer.h"

namespace ui {

//
// NSAccessibilityElement or AXUIElement accessible node comparator.
struct AXNodeComparator {
  constexpr bool operator()(const gfx::NativeViewAccessible& lhs,
                            const gfx::NativeViewAccessible& rhs) const {
    if (IsAXUIElement(lhs)) {
      DCHECK(IsAXUIElement(rhs));
      return CFHash(lhs) < CFHash(rhs);
    }
    DCHECK(IsNSAccessibilityElement(lhs));
    DCHECK(IsNSAccessibilityElement(rhs));
    return lhs < rhs;
  }
};

//
// NSAccessibility tree indexer.
using AXTreeIndexerMac = AXTreeIndexer<const gfx::NativeViewAccessible,
                                       GetDOMId,
                                       NSArray*,
                                       AXChildrenOf,
                                       AXNodeComparator>;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_
