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
  bool operator()(const gfx::NativeViewAccessible& lhs,
                  const gfx::NativeViewAccessible& rhs) const {
    id<NSAccessibility> id_lhs = lhs.Get();
    id<NSAccessibility> id_rhs = rhs.Get();
    if (AXElementWrapper::IsAXUIElement(id_lhs)) {
      CHECK(AXElementWrapper::IsAXUIElement(id_rhs));
      return CFHash((__bridge CFTypeRef)id_lhs) <
             CFHash((__bridge CFTypeRef)id_rhs);
    }
    CHECK(AXElementWrapper::IsNSAccessibilityElement(id_lhs));
    CHECK(AXElementWrapper::IsNSAccessibilityElement(id_rhs));
    return id_lhs < id_rhs;
  }
};

using AXTreeIndexerMacBase = AXTreeIndexer<gfx::NativeViewAccessible,
                                           AXElementWrapper::DOMIdOf,
                                           AXElementWrapper::ChildrenOf,
                                           AXNodeComparator>;

//
// NSAccessibility tree indexer.
class AXTreeIndexerMac : public AXTreeIndexerMacBase {
 public:
  explicit AXTreeIndexerMac(const gfx::NativeViewAccessible node)
      : AXTreeIndexer(node), type_(AXElementWrapper::TypeOf(node.Get())) {}

  std::string IndexBy(const gfx::NativeViewAccessible node) const override;

 private:
  AXElementWrapper::AXType type_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_MAC_H_
