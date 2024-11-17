// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_

#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer.h"

namespace ui {

// NSAccessibilityElement or AXUIElement accessible node comparator.
struct AXNodeComparator {
  constexpr bool operator()(const gfx::NativeViewAccessible& lhs,
                            const gfx::NativeViewAccessible& rhs) const {
    if (AXElementWrapper::IsAXUIElement(lhs)) {
      DCHECK(AXElementWrapper::IsAXUIElement(rhs));
      return CFHash((__bridge CFTypeRef)lhs) < CFHash((__bridge CFTypeRef)rhs);
    }
    DCHECK(AXElementWrapper::IsNSAccessibilityElement(lhs));
    DCHECK(AXElementWrapper::IsNSAccessibilityElement(rhs));
    return lhs < rhs;
  }
};

using AXTreeIndexerMacBase = AXTreeIndexer<const gfx::NativeViewAccessible,
                                           AXElementWrapper::DOMIdOf,
                                           NSArray*,
                                           AXElementWrapper::ChildrenOf,
                                           AXNodeComparator>;

//
// NSAccessibility tree indexer.
class AXTreeIndexerMac : public AXTreeIndexerMacBase {
 public:
  explicit AXTreeIndexerMac(const gfx::NativeViewAccessible node)
      : AXTreeIndexer(node), type_(AXElementWrapper::TypeOf(node)) {}

  std::string IndexBy(const gfx::NativeViewAccessible node) const override;

 private:
  AXElementWrapper::AXType type_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_
