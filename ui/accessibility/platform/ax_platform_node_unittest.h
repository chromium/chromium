// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_ax_tree_update.h"
#include "ui/accessibility/test_single_ax_tree_manager.h"

namespace ui {

class AXPlatformNodeTest : public ::testing::Test,
                           public TestSingleAXTreeManager {
 public:
  AXPlatformNodeTest();
  ~AXPlatformNodeTest() override;
  AXPlatformNodeTest(const AXPlatformNodeTest&) = delete;
  AXPlatformNodeTest& operator=(const AXPlatformNodeTest&) = delete;

 protected:
  void TearDown() override;

  AXTreeUpdate BuildTextField();
  AXTreeUpdate BuildTextFieldWithSelectionRange(int32_t start, int32_t stop);
  AXTreeUpdate BuildContentEditable();
  AXTreeUpdate BuildContentEditableWithSelectionRange(int32_t start,
                                                      int32_t end);
  AXTreeUpdate Build3X3Table();
  AXTreeUpdate BuildAriaColumnAndRowCountGrids();

  AXTreeUpdate BuildListBox(
      bool option_1_is_selected,
      bool option_2_is_selected,
      bool option_3_is_selected,
      const std::vector<ax::mojom::State>& additional_state);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_UNITTEST_H_
