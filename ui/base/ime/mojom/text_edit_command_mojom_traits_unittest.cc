// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojom/text_edit_command_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mojom/text_edit_commands.mojom.h"
#include "ui/base/ime/text_edit_commands.h"

namespace ui {

// This test checks ui::TextEditCommand is converted correctly to and from
// ui::mojom::TextEditCommand.
TEST(TextEditCommandTraitsTest, TextEditCommandTest) {
  const TextEditCommand kTextEditCommandTypes[] = {
#define TEXT_EDIT_COMMAND(UI, MOJOM) ui::TextEditCommand::UI,
#include "ui/base/ime/text_edit_commands.inc"
#undef TEXT_EDIT_COMMAND
  };

  for (auto type : kTextEditCommandTypes) {
    TextEditCommand out_command;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<ui::mojom::TextEditCommand>(
        type, out_command));
    ASSERT_EQ(type, out_command);
  }
}

}  // namespace ui
