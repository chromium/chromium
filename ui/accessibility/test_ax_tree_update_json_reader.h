// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_
#define UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_

#include "base/values.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

// This function assumes that the JSON input is properly formatted and any
// error in parsing can result in a runtime error.
// The JSON format is based on the output of the following code and should
// stay in sync with that:
// http://google3/knowledge/cerebra/sense/im2query/screenai/screen2x/crawl/web/crawler.ts
AXTreeUpdate AXTreeUpdateFromJSON(const base::Value& json);

ax::mojom::Role RoleFromStringForTesting(std::string role);
}  // namespace ui
#endif  // UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_
