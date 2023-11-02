// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/wayland/wayland_client_input_types.h"
#include "ui/base/wayland/wayland_server_input_types.h"

namespace ui::wayland {

TEST(WaylandInputTypesTest, TextInputType) {
  // Checking that roundtrip does not change the value.
  for (int i = 0; i <= TEXT_INPUT_TYPE_MAX; ++i) {
    const auto input_type = static_cast<ui::TextInputType>(i);
    EXPECT_EQ(input_type,
              ConvertToTextInputType(ConvertFromTextInputType(input_type)));
  }

  // Passing the invalid value returns nullopt.
  auto kInvalidValue =
      static_cast<zcr_extended_text_input_v1_input_type>(0xFFFFFFFF);
  EXPECT_FALSE(ConvertToTextInputType(kInvalidValue).has_value());
}

TEST(WaylandInputTypesTest, TextInputMode) {
  // Checking that roundtrip does not change the value.
  for (int i = 0; i <= TEXT_INPUT_MODE_MAX; ++i) {
    const auto input_mode = static_cast<ui::TextInputMode>(i);
    EXPECT_EQ(input_mode,
              ConvertToTextInputMode(ConvertFromTextInputMode(input_mode)));
  }

  // Passing the invalid value returns nullopt.
  auto kInvalidValue =
      static_cast<zcr_extended_text_input_v1_input_mode>(0xFFFFFFFF);
  EXPECT_FALSE(ConvertToTextInputMode(kInvalidValue).has_value());
}

}  // namespace ui::wayland
