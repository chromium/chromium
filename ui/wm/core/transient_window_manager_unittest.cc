// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/wm/core/transient_window_manager.h"
#include "base/memory/raw_ptr.h"

#include <utility>

#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

using aura::test::ChildWindowIDsAsString;
using aura::test::CreateTestWindowWithId;

namespace wm {

class TestTransientWindowObserver : public TransientWindowObserver {
 public:
  TestTransientWindowObserver() = default;

  TestTransientWindowObserver(const TestTransientWindowObserver&) = delete;
  TestTransientWindowObserver& operator=(const TestTransientWindowObserver&) =
      delete;

  ~TestTransientWindowObserver() override {}

  int add_count() const { return add_count_; }
  int remove_count() const { return remove_count_; }
  int parent_change_count() const { return parent_change_count_; }

  // TransientWindowObserver overrides:
  void OnTransientChildAdded(Window* window, Window* transient) override {
    add_count_++;
  }
  void OnTransientChildRemoved(Window* window, Window* transient) override {
    remove_count_++;
  }
  void OnTransientParentChanged(Window* window) override {
    parent_change_count_++;
  }

 private:
  int add_count_ = 0;
  int remove_count_ = 0;
  int parent_change_count_ = 0;
};

class WindowVisibilityObserver : public aura::WindowObserver {
 public:
  WindowVisibilityObserver(Window* observed_window,
                           std::unique_ptr<Window> owned_window)
      : observed_window_(observed_window),
        owned_window_(std::move(owned_window)) {
    observed_window_->AddObserver(this);
  }

  WindowVisibilityObserver(const WindowVisibilityObserver&) = delete;
  WindowVisibilityObserver& operator=(const WindowVisibilityObserver&) = delete;

  ~WindowVisibilityObserver() override {
    observed_window_->RemoveObserver(this);
  }

  void OnWindowVisibilityChanged(Window* window, bool visible) override {
    owned_window_.reset();
  }
 private:
  raw_ptr<Window> observed_window_;
  std::unique_ptr<Window> owned_window_;
};

class TransientWindowManagerTest : public aura::test::AuraTestBase {
 public:
  TransientWindowManagerTest() {}

  TransientWindowManagerTest(const TransientWindowManagerTest&) = delete;
  TransientWindowManagerTest& operator=(const TransientWindowManagerTest&) =
      delete;

  ~TransientWindowManagerTest() override {}

 protected:
  // Creates a transient window that is transient to |parent|.
  Window* CreateTransientChild(int id, Window* parent) {
    Window* window = new Window(NULL);
    window->SetId(id);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    AddTransientChild(parent, window);
    aura::client::ParentWindowWithContext(window, root_window(), gfx::Rect(),
                                          display::kInvalidDisplayId);
    return window;
  }
};

// Tests that creating a transient tree with a cycle in it will crash on a
// CHECK. See a crash that can happen if we allow cycles http://b/286947509.
TEST_F(TransientWindowManagerTest, TransientCycle) {
  std::unique_ptr<Window> w1(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w2(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> w3(CreateTestWindowWithId(2, root_window()));

  // Creating a cylce in the hierarchy will cause a crash.
  //
  // w1 <-- w2 <-- w3
  //  |             ^
  //  |             |
  //  +-------------+
  //
  wm::AddTransientChild(w1.get(), w2.get());
  wm::AddTransientChild(w2.get(), w3.get());
  EXPECT_DEATH(wm::AddTransientChild(w3.get(), w1.get()), "");
}

// Various assertions for transient children.
TEST_F(TransientWindowManagerTest, TransientChildren) {
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  std::unique_ptr<Window> w3(CreateTestWindowWithId(3, parent.get()));
  Window* w2 = CreateTestWindowWithId(2, parent.get());
  // w2 is now owned by w1.
  AddTransientChild(w1.get(), w2);
  // Stack w1 at the top (end), this should force w2 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = NULL;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);

  w1.reset(CreateTestWindowWithId(4, parent.get()));
  w2 = CreateTestWindowWithId(5, w3.get());
  AddTransientChild(w1.get(), w2);
  parent->StackChildAtTop(w3.get());
  // Stack w1 at the top (end), this shouldn't affect w2 since it has a
  // different parent.
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
  EXPECT_EQ(w1.get(), parent->children()[1]);

  // Hiding parent should hide transient children.
  EXPECT_TRUE(w2->IsVisible());
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());

  // And they should stay hidden even after the parent became visible.
  w1->Show();
  EXPECT_FALSE(w2->IsVisible());

  // Hidden transient child should stay hidden regardless of
  // parent's visibility.
  w2->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w1->Show();
  EXPECT_FALSE(w2->IsVisible());

  // Transient child can be shown even if the transient parent is hidden.
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w2->Show();
  EXPECT_TRUE(w2->IsVisible());
  w1->Show();
  EXPECT_TRUE(w2->IsVisible());

  // When the parent_controls_visibility is true, TransientWindowManager
  // controls the children's visibility. It stays invisible even if
  // Window::Show() is called, and gets shown when the parent becomes visible.
  wm::TransientWindowManager::GetOrCreate(w2)->set_parent_controls_visibility(
      true);
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w2->Show();
  EXPECT_FALSE(w2->IsVisible());
  w1->Show();
  EXPECT_TRUE(w2->IsVisible());

  // Hiding a transient child that is hidden by the transient parent
  // is not currently handled and will be shown anyway.
  w1->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w2->Hide();
  EXPECT_FALSE(w2->IsVisible());
  w1->Show();
  EXPECT_TRUE(w2->IsVisible());
}

// Tests that transient children are stacked as a unit when using stack above.
TEST_F(TransientWindowManagerTest, TransientChildrenGroupAbove) {
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

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
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
TEST_F(TransientWindowManagerTest, TransientChildrenGroupBelow) {
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

  // w11 is now owned by w1.
  AddTransientChild(w1.get(), w11);
  // w21 is now owned by w2.
  AddTransientChild(w2.get(), w21);
  // w22 is now owned by w2.
  AddTransientChild(w2.get(), w22);
  // w211 is now owned by w21.
  AddTransientChild(w21, w211);
  // w212 is now owned by w21.
  AddTransientChild(w21, w212);
  // w213 is now owned by w21.
  AddTransientChild(w21, w213);
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

// Tests that transient windows are stacked properly when created.
TEST_F(TransientWindowManagerTest, StackUponCreation) {
  std::unique_ptr<Window> window0(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));

  std::unique_ptr<Window> window2(CreateTransientChild(2, window0.get()));
  EXPECT_EQ("0 2 1", ChildWindowIDsAsString(root_window()));
}

// Tests for a crash when window destroyed inside
// UpdateTransientChildVisibility loop.
TEST_F(TransientWindowManagerTest, CrashOnVisibilityChange) {
  std::unique_ptr<Window> window1(CreateTransientChild(1, root_window()));
  std::unique_ptr<Window> window2(CreateTransientChild(2, root_window()));
  window1->Show();
  window2->Show();

  WindowVisibilityObserver visibility_observer(window1.get(),
                                               std::move(window2));
  root_window()->Hide();
}
// Tests that windows are restacked properly after a call to AddTransientChild()
// or RemoveTransientChild().
TEST_F(TransientWindowManagerTest, RestackUponAddOrRemoveTransientChild) {
  std::unique_ptr<Window> windows[4];
  for (int i = 0; i < 4; i++)
    windows[i].reset(CreateTestWindowWithId(i, root_window()));
  EXPECT_EQ("0 1 2 3", ChildWindowIDsAsString(root_window()));

  AddTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 2 1 3", ChildWindowIDsAsString(root_window()));

  AddTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 2 3 1", ChildWindowIDsAsString(root_window()));

  RemoveTransientChild(windows[0].get(), windows[2].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(root_window()));

  RemoveTransientChild(windows[0].get(), windows[3].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(root_window()));
}

namespace {

// Used by NotifyDelegateAfterDeletingTransients. Adds a string to a vector when
// OnWindowDestroyed() is invoked so that destruction order can be verified.
class DestroyedTrackingDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit DestroyedTrackingDelegate(const std::string& name,
                                     std::vector<std::string>* results)
      : name_(name),
        results_(results) {}

  DestroyedTrackingDelegate(const DestroyedTrackingDelegate&) = delete;
  DestroyedTrackingDelegate& operator=(const DestroyedTrackingDelegate&) =
      delete;

  void OnWindowDestroyed(aura::Window* window) override {
    results_->push_back(name_);
  }

 private:
  const std::string name_;
  raw_ptr<std::vector<std::string>> results_;
};

}  // namespace

// Verifies the delegate is notified of destruction after transients are
// destroyed.
TEST_F(TransientWindowManagerTest, NotifyDelegateAfterDeletingTransients) {
  std::vector<std::string> destruction_order;

  DestroyedTrackingDelegate parent_delegate("parent", &destruction_order);
  std::unique_ptr<Window> parent(new Window(&parent_delegate));
  parent->Init(ui::LAYER_NOT_DRAWN);

  DestroyedTrackingDelegate transient_delegate("transient", &destruction_order);
  Window* transient = new Window(&transient_delegate);  // Owned by |parent|.
  transient->Init(ui::LAYER_NOT_DRAWN);
  AddTransientChild(parent.get(), transient);
  parent.reset();

  ASSERT_EQ(2u, destruction_order.size());
  EXPECT_EQ("transient", destruction_order[0]);
  EXPECT_EQ("parent", destruction_order[1]);
}

TEST_F(TransientWindowManagerTest,
       StackTransientsLayersRelativeToOtherTransients) {
  // Create a window with several transients, then a couple windows on top.
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> window11(CreateTransientChild(11, window1.get()));
  std::unique_ptr<Window> window12(CreateTransientChild(12, window1.get()));
  std::unique_ptr<Window> window13(CreateTransientChild(13, window1.get()));

  EXPECT_EQ("1 11 12 13", ChildWindowIDsAsString(root_window()));

  // Stack 11 above 12.
  root_window()->StackChildAbove(window11.get(), window12.get());
  EXPECT_EQ("1 12 11 13", ChildWindowIDsAsString(root_window()));

  // Stack 13 below 12.
  root_window()->StackChildBelow(window13.get(), window12.get());
  EXPECT_EQ("1 13 12 11", ChildWindowIDsAsString(root_window()));

  // Stack 11 above 1.
  root_window()->StackChildAbove(window11.get(), window1.get());
  EXPECT_EQ("1 11 13 12", ChildWindowIDsAsString(root_window()));

  // Stack 12 below 13.
  root_window()->StackChildBelow(window12.get(), window13.get());
  EXPECT_EQ("1 11 12 13", ChildWindowIDsAsString(root_window()));
}

// Verifies TransientWindowObserver is notified appropriately.
TEST_F(TransientWindowManagerTest, TransientWindowObserverNotified) {
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));

  TestTransientWindowObserver test_parent_observer, test_child_observer;
  TransientWindowManager::GetOrCreate(parent.get())
      ->AddObserver(&test_parent_observer);
  TransientWindowManager::GetOrCreate(w1.get())->AddObserver(
      &test_child_observer);

  AddTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_parent_observer.add_count());
  EXPECT_EQ(0, test_parent_observer.remove_count());
  EXPECT_EQ(1, test_child_observer.parent_change_count());

  RemoveTransientChild(parent.get(), w1.get());
  EXPECT_EQ(1, test_parent_observer.add_count());
  EXPECT_EQ(1, test_parent_observer.remove_count());
  EXPECT_EQ(2, test_child_observer.parent_change_count());

  TransientWindowManager::GetOrCreate(parent.get())
      ->RemoveObserver(&test_parent_observer);
  TransientWindowManager::GetOrCreate(parent.get())
      ->RemoveObserver(&test_child_observer);
}

TEST_F(TransientWindowManagerTest, ChangeParent) {
  std::unique_ptr<Window> container_1(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> container_2(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> container_3(CreateTestWindowWithId(2, root_window()));
  std::unique_ptr<Window> parent(CreateTestWindowWithId(3, container_1.get()));
  std::unique_ptr<Window> child_1(CreateTestWindowWithId(4, container_1.get()));
  std::unique_ptr<Window> child_2(CreateTestWindowWithId(5, container_1.get()));
  std::unique_ptr<Window> child_3(CreateTestWindowWithId(6, container_1.get()));
  std::unique_ptr<Window> child_4(CreateTestWindowWithId(7, container_3.get()));
  std::unique_ptr<Window> child_5(CreateTestWindowWithId(8, container_1.get()));

  AddTransientChild(parent.get(), child_1.get());
  AddTransientChild(child_1.get(), child_2.get());
  AddTransientChild(parent.get(), child_4.get());
  AddTransientChild(parent.get(), child_5.get());

  container_2->AddChild(parent.get());
  // Transient children on the old container should be reparented to the new
  // container.
  EXPECT_EQ(child_1->parent(), container_2.get());
  EXPECT_EQ(child_2->parent(), container_2.get());
  EXPECT_EQ(child_5->parent(), container_2.get());
  // child_3 and child_4 should remain unaffected.
  EXPECT_EQ(child_3->parent(), container_1.get());
  EXPECT_EQ(child_4->parent(), container_3.get());
}

// Tests that the lifetime of the transient window will be determined by its
// transient parent by default. But the transient window is still able to
// outlive the transient parent if we explicitly
// `set_parent_controls_lifetime()` to false through its transient window
// manager.
TEST_F(TransientWindowManagerTest,
       TransientLifeTimeMayBeControlledByTransientParent) {
  // Test that the lifetime of the transient window is controlled by its
  // transient parent by default.
  std::unique_ptr<Window> parent(CreateTestWindowWithId(0, root_window()));
  std::unique_ptr<Window> transient(CreateTransientChild(1, parent.get()));

  aura::WindowTracker tracker({transient.get()});

  // Release the ownership of the `transient` to avoid double deletion in
  // `TransientWindowManager::OnWindowDestroying()`.
  transient.release();
  parent.reset();
  EXPECT_TRUE(tracker.windows().empty());

  std::unique_ptr<Window> new_parent(CreateTestWindowWithId(2, root_window()));
  std::unique_ptr<Window> new_transient(
      CreateTransientChild(3, new_parent.get()));

  tracker.Add(new_transient.get());

  // Test that the transient window can outlive its transient parent by setting
  // `set_parent_controls_lifetime()` to false.
  wm::TransientWindowManager::GetOrCreate(new_transient.get())
      ->set_parent_controls_lifetime(false);
  new_parent.reset();
  EXPECT_FALSE(tracker.windows().empty());
}

}  // namespace wm
