// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_stacking_client.h"

#include <memory>

#include "base/macros.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/wm/core/window_util.h"

using aura::test::ChildWindowIDsAsString;
using aura::test::CreateTestWindowWithId;
using aura::Window;

namespace wm {

class TransientWindowStackingClientTest : public aura::test::AuraTestBase {
 public:
  TransientWindowStackingClientTest() {}
  ~TransientWindowStackingClientTest() override {}

  void SetUp() override {
    AuraTestBase::SetUp();
    client_ = std::make_unique<TransientWindowStackingClient>();
    aura::client::SetWindowStackingClient(client_.get());
  }

  void TearDown() override {
    aura::client::SetWindowStackingClient(NULL);
    AuraTestBase::TearDown();
  }

 private:
  std::unique_ptr<TransientWindowStackingClient> client_;
  DISALLOW_COPY_AND_ASSIGN(TransientWindowStackingClientTest);
};

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowStackingClientTest, TransientChildrenGroupAbove) {
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  Window* w11 = CreateTestWindowWithId(11, parent.get());
  std::unique_ptr<Window> w2(CreateTestWindowWithId(2, parent.get()));
  Window* w21 = CreateTestWindowWithId(21, parent.get());
  Window* w211 = CreateTestWindowWithId(211, parent.get());
  Window* w212 = CreateTestWindowWithId(212, parent.get());
  Window* w213 = CreateTestWindowWithId(213, parent.get());
  Window* w22 = CreateTestWindowWithId(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  AddTransientChild(w1.get(), w11);  // w11 is now owned by w1.
  AddTransientChild(w2.get(), w21);  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w22);  // w22 is now owned by w2.
  AddTransientChild(w21, w211);  // w211 is now owned by w21.
  AddTransientChild(w21, w212);  // w212 is now owned by w21.
  AddTransientChild(w21, w213);  // w213 is now owned by w21.
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w1 at the top (end), this should force w11 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtTop(w2.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w2.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w21, w1.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w21, w22);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w21);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w213, w21);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent above its transient child.
  parent->StackChildAbove(w21, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAbove(w2.get(), w1.get());
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAbove(w11, w213);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient children are stacked as a unit when using stack below.
TEST_F(TransientWindowStackingClientTest, TransientChildrenGroupBelow) {
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  Window* w11 = CreateTestWindowWithId(11, parent.get());
  std::unique_ptr<Window> w2(CreateTestWindowWithId(2, parent.get()));
  Window* w21 = CreateTestWindowWithId(21, parent.get());
  Window* w211 = CreateTestWindowWithId(211, parent.get());
  Window* w212 = CreateTestWindowWithId(212, parent.get());
  Window* w213 = CreateTestWindowWithId(213, parent.get());
  Window* w22 = CreateTestWindowWithId(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  AddTransientChild(w1.get(), w11);  // w11 is now owned by w1.
  AddTransientChild(w2.get(), w21);  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w22);  // w22 is now owned by w2.
  AddTransientChild(w21, w211);  // w211 is now owned by w21.
  AddTransientChild(w21, w212);  // w212 is now owned by w21.
  AddTransientChild(w21, w213);  // w213 is now owned by w21.
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w2 at the bottom, this should force w11 to be last (on top of w1).
  // This also tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  parent->StackChildAtBottom(w2.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildAtBottom(w1.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w21, w1.get());
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w11, w2.get());
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w22, w21);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w21, w11);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w213, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent below its transient child.
  parent->StackChildBelow(w21, w211);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w1.get(), w2.get());
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  parent->StackChildBelow(w213, w11);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that windows can be stacked above windows with a NULL layer delegate.
// Windows have a NULL layer delegate when they are in the process of closing.
// See crbug.com/443433
TEST_F(TransientWindowStackingClientTest,
       StackAboveWindowWithNULLLayerDelegate) {
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  std::unique_ptr<Window> w2(CreateTestWindowWithId(2, parent.get()));
  w2->layer()->set_delegate(NULL);
  EXPECT_EQ(w2.get(), parent->children().back());

  parent->StackChildAbove(w1.get(), w2.get());
  EXPECT_EQ(w1.get(), parent->children().back());
}

}  // namespace wm
