// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_tree_indexer_mac.h"

namespace ui {

std::string AXTreeIndexerMac::IndexBy(
    const gfx::NativeViewAccessible node) const {
  // There are two types of trees in macOS accessibility: the internal
  // NSAccessibility tree and the external AXUIElement tree. If a node
  // from another tree (other than the root for which the indexer was
  // created) is given, communicate this by returning a special value
  // instead of crashing at DCHECKs later in the code. This may happen,
  // for example, when text markers, which binary serialization always
  // points to the internal tree, are stringified while running through
  // the external tree.
  if (AXElementWrapper::TypeOf(node) != type_) {
    return ":outsider";
  }
  return AXTreeIndexerMacBase::IndexBy(node);
}

}  // namespace ui
