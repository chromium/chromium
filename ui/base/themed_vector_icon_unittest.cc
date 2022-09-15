// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/themed_vector_icon.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {

const gfx::VectorIcon* GetVectorIcon() {
  static constexpr gfx::PathElement path[] = {gfx::CommandType::CIRCLE, 24, 18,
                                              5};
  static const gfx::VectorIconRep rep[] = {{path, 4}};
  static constexpr gfx::VectorIcon circle_icon = {rep, 1, "circle"};

  return &circle_icon;
}

}  // namespace

TEST(ThemedVectorIconTest, DefaultEmpty) {
  ThemedVectorIcon vector_icon;

  EXPECT_TRUE(vector_icon.empty());
}

TEST(ThemedVectorIconTest, CheckForVectorIcon) {
  ThemedVectorIcon vector_icon = ThemedVectorIcon(GetVectorIcon());

  EXPECT_FALSE(vector_icon.empty());
}

TEST(ImageModelTest, CheckAssign) {
  ThemedVectorIcon vector_icon_dest;
  ThemedVectorIcon vector_icon_src(GetVectorIcon());

  EXPECT_TRUE(vector_icon_dest.empty());
  EXPECT_FALSE(vector_icon_src.empty());

  vector_icon_dest = vector_icon_src;
  EXPECT_FALSE(vector_icon_dest.empty());
}

}  // namespace ui
