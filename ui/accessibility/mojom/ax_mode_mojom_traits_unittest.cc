// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_mode_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/mojom/ax_mode.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXModeMojomTraitsTest, TestSerializeAndDeserializeAXModeData) {
  ui::AXMode input(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
  ui::AXMode output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXMode>(input, output));
  EXPECT_EQ(ui::kAXModeBasic, output);
}
