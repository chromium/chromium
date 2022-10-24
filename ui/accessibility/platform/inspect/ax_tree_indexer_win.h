// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_WIN_H_

#include <wrl/client.h>

#include "base/values.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer.h"

namespace ui {

using AXTreeIndexerWin =
    AXTreeIndexer<Microsoft::WRL::ComPtr<IAccessible>,
                  GetDOMId,
                  std::vector<Microsoft::WRL::ComPtr<IAccessible>>,
                  IAccessibleChildrenOf>;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_INDEXER_WIN_H_
