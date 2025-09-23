// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_stacking_client.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/wm/core/window_util.h"

using aura::Window;
using aura::test::ChildWindowIDsAsString;
using aura::test::CreateTestWindow;

namespace wm {

class TransientWindowStackingClientTest : public aura::test::AuraTestBase {
 public:
  TransientWindowStackingClientTest() {}

  TransientWindowStackingClientTest(const TransientWindowStackingClientTest&) =
      delete;
  TransientWindowStackingClientTest& operator=(
      const TransientWindowStackingClientTest&) = delete;

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
};

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowStackingClientTest, TransientChildrenGroupAbove) {
  std::unique_ptr<Window> parent = CreateTestWindow(
      {.parent = root_window(), .bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<Window> w1 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 1});
  Window* w11 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 11})
          .release();
  std::unique_ptr<Window> w2 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 2});
  Window* w21 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 21})
          .release();
  Window* w211 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 211})
          .release();
  Window* w212 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 212})
          .release();
  Window* w213 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 213})
          .release();
  Window* w22 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 22})
          .release();
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
  std::unique_ptr<Window> parent = CreateTestWindow(
      {.parent = root_window(), .bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<Window> w1 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 1});
  Window* w11 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 11})
          .release();
  std::unique_ptr<Window> w2 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 2});
  Window* w21 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 21})
          .release();
  Window* w211 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 211})
          .release();
  Window* w212 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 212})
          .release();
  Window* w213 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 213})
          .release();
  Window* w22 =
      CreateTestWindow(
          {.parent = parent.get(), .bounds = {100, 100}, .window_id = 22})
          .release();
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
  std::unique_ptr<Window> parent = CreateTestWindow(
      {.parent = root_window(), .bounds = {100, 100}, .window_id = 0});
  std::unique_ptr<Window> w1 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 1});
  std::unique_ptr<Window> w2 = CreateTestWindow(
      {.parent = parent.get(), .bounds = {100, 100}, .window_id = 2});
  w2->layer()->set_delegate(NULL);
  EXPECT_EQ(w2.get(), parent->children().back());

  parent->StackChildAbove(w1.get(), w2.get());
  EXPECT_EQ(w1.get(), parent->children().back());
}

}  // namespace wm
