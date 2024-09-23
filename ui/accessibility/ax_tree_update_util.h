// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_UPDATE_UTIL_H_
#define UI_ACCESSIBILITY_AX_TREE_UPDATE_UTIL_H_

#include <vector>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace ui {

AX_EXPORT bool AXTreeUpdatesCanBeMerged(const AXTreeUpdate& u1,
                                        const AXTreeUpdate& u2);

// If 2 or more tree updates can all be merged into others,
// process the whole set of tree updates, copying them to |dst|,
// and returning true.  Otherwise, return false and |dst|
// is left unchanged.
//
// Merging tree updates helps minimize the overhead of calling
// Unserialize multiple times.
AX_EXPORT bool MergeAXTreeUpdates(const std::vector<AXTreeUpdate>& src,
                                  std::vector<AXTreeUpdate>* dst);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_UPDATE_UTIL_H_
