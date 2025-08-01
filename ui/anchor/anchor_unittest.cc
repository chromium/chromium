// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/anchor/anchor.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

namespace {

class TestAnchorImpl : public AnchorImpl {
 public:
  TestAnchorImpl() = default;
  ~TestAnchorImpl() override = default;

  // AnchorImpl:
  std::unique_ptr<AnchorImpl> Clone() const override {
    return std::make_unique<TestAnchorImpl>();
  }
  bool IsEmpty() const override { return false; }
  gfx::Rect GetScreenBounds() const override { return gfx::Rect(1, 2, 3, 4); }
  views::Widget* GetWidget() override { return nullptr; }
  bool IsView() const override { return false; }
  views::View* GetView() override { return nullptr; }
};

// A derived class to access the protected constructor of Anchor.
class TestAnchor : public Anchor {
 public:
  TestAnchor() : Anchor(std::make_unique<TestAnchorImpl>()) {}
};

}  // namespace

TEST(AnchorTest, EmptyAnchor) {
  Anchor empty_anchor;
  EXPECT_TRUE(empty_anchor.IsEmpty());
  EXPECT_FALSE(empty_anchor);
  EXPECT_FALSE(empty_anchor.IsView());
  EXPECT_EQ(nullptr, empty_anchor.GetView());
  EXPECT_EQ(gfx::Rect(), empty_anchor.GetScreenBounds());
  EXPECT_EQ(nullptr, empty_anchor.GetWidget());
}

TEST(AnchorTest, CopyConstructor) {
  // Copy empty anchor.
  Anchor empty_anchor;
  Anchor empty_anchor_copy(empty_anchor);
  EXPECT_TRUE(empty_anchor_copy.IsEmpty());

  // Copy non-empty anchor.
  TestAnchor test_anchor;
  Anchor test_anchor_copy(test_anchor);
  EXPECT_FALSE(test_anchor_copy.IsEmpty());
  EXPECT_EQ(test_anchor.GetScreenBounds(), test_anchor_copy.GetScreenBounds());
}

TEST(AnchorTest, MoveConstructor) {
  // Move empty anchor.
  Anchor empty_anchor;
  Anchor empty_anchor_moved(std::move(empty_anchor));
  EXPECT_TRUE(empty_anchor_moved.IsEmpty());

  // Move non-empty anchor.
  TestAnchor test_anchor;
  gfx::Rect bounds = test_anchor.GetScreenBounds();
  Anchor test_anchor_moved(std::move(test_anchor));
  EXPECT_FALSE(test_anchor_moved.IsEmpty());
  EXPECT_EQ(bounds, test_anchor_moved.GetScreenBounds());

  // Test that the moved-from anchor is now empty.
  EXPECT_TRUE(test_anchor.IsEmpty());
  EXPECT_EQ(gfx::Rect(), test_anchor.GetScreenBounds());
}

TEST(AnchorTest, CopyAssignment) {
  // Copy-assign empty anchor to non-empty.
  Anchor test_anchor = TestAnchor();
  Anchor empty_anchor;
  test_anchor = empty_anchor;
  EXPECT_TRUE(test_anchor.IsEmpty());

  // Copy-assign non-empty anchor to empty.
  TestAnchor test_anchor2;
  empty_anchor = test_anchor2;
  EXPECT_FALSE(empty_anchor.IsEmpty());
  EXPECT_EQ(test_anchor2.GetScreenBounds(), empty_anchor.GetScreenBounds());
}

TEST(AnchorTest, MoveAssignment) {
  // Move-assign empty anchor to non-empty.
  Anchor test_anchor = TestAnchor();
  Anchor empty_anchor;
  test_anchor = std::move(empty_anchor);
  EXPECT_TRUE(test_anchor.IsEmpty());

  // Move-assign non-empty anchor to empty.
  Anchor empty_anchor2;
  TestAnchor test_anchor2;
  gfx::Rect bounds = test_anchor2.GetScreenBounds();
  empty_anchor2 = std::move(test_anchor2);
  EXPECT_FALSE(empty_anchor2.IsEmpty());
  EXPECT_EQ(bounds, empty_anchor2.GetScreenBounds());
  EXPECT_TRUE(test_anchor2.IsEmpty());
}

}  // namespace ui
