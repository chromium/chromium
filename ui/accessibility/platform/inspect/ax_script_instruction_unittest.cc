// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

TEST(AXScriptInstructionTest, Parse) {
  AXScriptInstruction script("textbox.AXRole");
  EXPECT_TRUE(script.IsScript());
  EXPECT_FALSE(script.IsEvent());
  EXPECT_FALSE(script.IsComment());
  EXPECT_EQ(script.AsScript().ToString(), "textbox.AXRole");

  AXScriptInstruction event("wait for AXTitleChange");
  EXPECT_TRUE(event.IsEvent());
  EXPECT_FALSE(event.IsScript());
  EXPECT_FALSE(event.IsComment());
  EXPECT_EQ(event.AsEvent(), "AXTitleChange");

  AXScriptInstruction event2("wait for AXTitleChange on AXButton");
  EXPECT_TRUE(event2.IsEvent());
  EXPECT_FALSE(event2.IsScript());
  EXPECT_FALSE(event2.IsComment());
  EXPECT_EQ(event2.AsEvent(), "AXTitleChange on AXButton");

  AXScriptInstruction comment("// wait for AXTitleChange");
  EXPECT_TRUE(comment.IsComment());
  EXPECT_FALSE(comment.IsEvent());
  EXPECT_FALSE(comment.IsScript());
  EXPECT_EQ(comment.AsComment(), "// wait for AXTitleChange");
}

}  // namespace ui
