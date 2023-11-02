// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

TEST(AXScriptInstructionTest, Parse) {
  AXScriptInstruction script("textbox.AXRole");
  EXPECT_TRUE(script.IsScript());
  EXPECT_FALSE(script.IsKeyEvent());
  EXPECT_FALSE(script.IsKeyEvent());
  EXPECT_FALSE(script.IsComment());
  EXPECT_FALSE(script.IsPrintTree());
  EXPECT_EQ(script.AsScript().ToString(), "textbox.AXRole");

  AXScriptInstruction event("wait for AXTitleChange");
  EXPECT_TRUE(event.IsEvent());
  EXPECT_FALSE(event.IsKeyEvent());
  EXPECT_FALSE(event.IsScript());
  EXPECT_FALSE(event.IsComment());
  EXPECT_FALSE(event.IsPrintTree());
  EXPECT_EQ(event.AsEvent(), "AXTitleChange");

  AXScriptInstruction event2("wait for AXTitleChange on AXButton");
  EXPECT_TRUE(event2.IsEvent());
  EXPECT_FALSE(event2.IsKeyEvent());
  EXPECT_FALSE(event2.IsScript());
  EXPECT_FALSE(event2.IsComment());
  EXPECT_FALSE(event2.IsPrintTree());
  EXPECT_EQ(event2.AsEvent(), "AXTitleChange on AXButton");

  AXScriptInstruction printTree("print tree");
  EXPECT_FALSE(printTree.IsEvent());
  EXPECT_FALSE(printTree.IsKeyEvent());
  EXPECT_FALSE(printTree.IsScript());
  EXPECT_FALSE(printTree.IsComment());
  EXPECT_TRUE(printTree.IsPrintTree());

  AXScriptInstruction comment("// wait for AXTitleChange");
  EXPECT_TRUE(comment.IsComment());
  EXPECT_FALSE(comment.IsEvent());
  EXPECT_FALSE(comment.IsKeyEvent());
  EXPECT_FALSE(comment.IsScript());
  EXPECT_FALSE(comment.IsPrintTree());
  EXPECT_EQ(comment.AsComment(), "// wait for AXTitleChange");

  AXScriptInstruction comment2("// press Enter");
  EXPECT_TRUE(comment2.IsComment());
  EXPECT_FALSE(comment2.IsEvent());
  EXPECT_FALSE(comment2.IsKeyEvent());
  EXPECT_FALSE(comment2.IsScript());
  EXPECT_FALSE(comment2.IsPrintTree());
  EXPECT_EQ(comment2.AsComment(), "// press Enter");

  AXScriptInstruction keypress("press Enter");
  EXPECT_TRUE(keypress.IsKeyEvent());
  EXPECT_FALSE(keypress.IsComment());
  EXPECT_FALSE(keypress.IsEvent());
  EXPECT_FALSE(keypress.IsScript());
  EXPECT_FALSE(keypress.IsPrintTree());
  EXPECT_EQ(keypress.AsDomKeyString(), "Enter");
}

}  // namespace ui
