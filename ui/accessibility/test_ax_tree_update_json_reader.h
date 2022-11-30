// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_
#define UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_

#include "base/values.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

// This function assumes that the JSON input is properly formatted and any
// error in parsing can result in a runtime error.
// The JSON format is based on the output of
// |InspectorAccessibilityAgent::WalkAXNodesToDepth| and should stay in sync
// with that.
// NOTE: This parser is not complete and only processes the required tags for
// the existing tests.
// |role_conversions| is a map of role strings in the JSON file to Chrome roles.
// TODO(https://crbug.com/1278249): Drop |role_conversions| once Chrome roles
// are added to the JSON file.
AXTreeUpdate AXTreeUpdateFromJSON(
    const base::Value& json,
    const std::map<std::string, ax::mojom::Role>* role_conversions);
}  // namespace ui
#endif  // UI_ACCESSIBILITY_TEST_AX_TREE_UPDATE_JSON_READER_H_
